#include "engine/SeasonManager.h"
#include "engine/MatchEngine.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <random>
#include <map>
#include <utility>
#include <numeric>
#include <cmath>
#include <unordered_set>
#include <optional>

namespace bm {
namespace engine {

SeasonManager::SeasonManager()
    : seasonYear(2025), leagueType("NBA"), gamesPlayed(0), totalGames(0), playoffsComplete(false) {
    matchEngine = std::make_shared<MatchEngine>();
    std::cout << "[SeasonManager] Initialized\n";
}

SeasonManager::~SeasonManager() {
}

void SeasonManager::InitializeSeason(int year, const std::string& league) {
    seasonYear = year;
    leagueType = league;
    gamesPlayed = 0;
    playoffsComplete = false;
    schedule.clear();
    standings.clear();
    
    std::cout << "[SeasonManager] Initializing " << league << " " << year << " season\n";
}

void SeasonManager::LoadTeams(const std::vector<std::shared_ptr<Team>>& teams) {
    allTeams = teams;
    
    // Initialize standings for each team
    for (auto& team : allTeams) {
        TeamRecord record;
        record.team = team;
        standings[team->teamId] = record;
    }
    
    std::cout << "[SeasonManager] Loaded " << allTeams.size() << " teams\n";
}

void SeasonManager::GenerateSchedule() {
    if (leagueType == "NBA") {
        GenerateNBASchedule();
    } else if (leagueType == "NCAA") {
        GenerateNCAASchedule();
    } else {
        std::cerr << "[SeasonManager] Unknown league type: " << leagueType << "\n";
    }
    
    totalGames = schedule.size();
    std::cout << "[SeasonManager] Generated " << totalGames << " games\n";
}

void SeasonManager::GenerateNBASchedule() {
    schedule.clear();
    fixtureDays.clear();

    const int targetGamesPerTeam = 82;
    const int totalTeams = static_cast<int>(allTeams.size());
    if (totalTeams < 2) {
        return;
    }

    std::vector<int> gamesRemaining(totalTeams, targetGamesPerTeam);
    std::vector<std::pair<int, int>> allPairs;
    for (int i = 0; i < totalTeams; ++i) {
        for (int j = i + 1; j < totalTeams; ++j) {
            allPairs.emplace_back(i, j);
        }
    }

    std::map<std::pair<int, int>, int> pairCounts;
    for (const auto& pair : allPairs) {
        pairCounts[pair] = 2;
        gamesRemaining[pair.first] -= 2;
        gamesRemaining[pair.second] -= 2;
    }

    for (const auto& pair : allPairs) {
        int i = pair.first;
        int j = pair.second;
        while (gamesRemaining[i] > 0 && gamesRemaining[j] > 0 && pairCounts[pair] < 4) {
            Game g;
            if (pairCounts[pair] % 2 == 0) {
                g.homeTeam = allTeams[i];
                g.awayTeam = allTeams[j];
            } else {
                g.homeTeam = allTeams[j];
                g.awayTeam = allTeams[i];
            }
            schedule.push_back(g);
            pairCounts[pair]++;
            gamesRemaining[i]--;
            gamesRemaining[j]--;
        }
    }

    // Fill remaining games while keeping every team at 82 games exactly.
    while (true) {
        bool done = true;
        for (int t = 0; t < totalTeams; ++t) {
            if (gamesRemaining[t] > 0) {
                done = false;
                break;
            }
        }
        if (done) break;

        for (int i = 0; i < totalTeams; ++i) {
            if (gamesRemaining[i] <= 0) continue;
            for (int j = i + 1; j < totalTeams; ++j) {
                if (gamesRemaining[j] <= 0) continue;
                if (pairCounts[{i, j}] >= 4) continue;

                Game g;
                if ((pairCounts[{i, j}] + 1) % 2 == 0) {
                    g.homeTeam = allTeams[i];
                    g.awayTeam = allTeams[j];
                } else {
                    g.homeTeam = allTeams[j];
                    g.awayTeam = allTeams[i];
                }
                schedule.push_back(g);
                pairCounts[{i, j}]++;
                gamesRemaining[i]--;
                gamesRemaining[j]--;

                if (gamesRemaining[i] <= 0 && gamesRemaining[j] <= 0) {
                    break;
                }
            }
        }
    }

    std::mt19937 gen(static_cast<unsigned int>(std::random_device{}()));
    std::shuffle(schedule.begin(), schedule.end(), gen);
    totalGames = static_cast<int>(schedule.size());
    BuildFixtureDays();
}

void SeasonManager::GenerateNCAASchedule() {
    schedule.clear();
    fixtureDays.clear();

    int gamesPerMatchup = 2;
    for (size_t i = 0; i < allTeams.size(); ++i) {
        for (size_t j = i + 1; j < allTeams.size(); ++j) {
            bool sameConference = (allTeams[i]->conferenceId == allTeams[j]->conferenceId);
            int games = sameConference ? gamesPerMatchup : 1;

            for (int game = 0; game < games; ++game) {
                Game g;
                if (game % 2 == 0) {
                    g.homeTeam = allTeams[i];
                    g.awayTeam = allTeams[j];
                } else {
                    g.homeTeam = allTeams[j];
                    g.awayTeam = allTeams[i];
                }
                schedule.push_back(g);
            }
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(schedule.begin(), schedule.end(), gen);
    totalGames = static_cast<int>(schedule.size());
    BuildFixtureDays();
}

void SeasonManager::BuildFixtureDays() {
    // Distribute schedule across fixture days ensuring a team does not play
    // more than once on the same day. Use an approximate day count so that
    // roughly half the teams play each day (one game per team).
    fixtureDays.clear();
    if (schedule.empty()) return;

    const int totalTeams = static_cast<int>(allTeams.size());
    const int gamesPerDayApprox = std::max(1, totalTeams / 2);
    int dayCount = std::max(30, static_cast<int>(std::ceil(static_cast<float>(schedule.size()) / gamesPerDayApprox)));

    fixtureDays.clear();
    fixtureDays.reserve(dayCount);

    // Prepare day containers and tracking sets
    for (int d = 0; d < dayCount; ++d) {
        FixtureDay fd;
        fd.dayNumber = d + 1;
        fd.label = "Day " + std::to_string(d + 1);
        fd.simulated = false;
        fixtureDays.push_back(std::move(fd));
    }

    std::vector<std::unordered_set<std::string>> teamsScheduled(dayCount);

    // Place each game into the earliest day where neither team is already
    // scheduled. If none available, add a new day.
    for (const auto& g : schedule) {
        bool placed = false;
        for (int d = 0; d < static_cast<int>(fixtureDays.size()); ++d) {
            auto &s = teamsScheduled[d];
            if (s.find(g.homeTeam->teamId) == s.end() && s.find(g.awayTeam->teamId) == s.end()) {
                fixtureDays[d].games.push_back(g);
                s.insert(g.homeTeam->teamId);
                s.insert(g.awayTeam->teamId);
                placed = true;
                break;
            }
        }

        if (!placed) {
            FixtureDay fd;
            fd.dayNumber = static_cast<int>(fixtureDays.size()) + 1;
            fd.label = "Day " + std::to_string(fd.dayNumber);
            fd.simulated = false;
            fd.games.push_back(g);
            fixtureDays.push_back(std::move(fd));

            std::unordered_set<std::string> s;
            s.insert(g.homeTeam->teamId);
            s.insert(g.awayTeam->teamId);
            teamsScheduled.push_back(std::move(s));
        }
    }
}

std::vector<SeasonManager::Game> SeasonManager::GetFixturesForDay(int dayNumber) const {
    const int index = std::max(0, dayNumber - 1);
    if (index >= static_cast<int>(fixtureDays.size())) {
        return {};
    }
    return fixtureDays[index].games;
}

bool SeasonManager::SimulateDay(int dayNumber, bool autoSimComputerGames, const std::shared_ptr<Team>& managedTeam, bool interactiveManagedGame, int speedMultiplier) {
    if (dayNumber < 1 || dayNumber > static_cast<int>(fixtureDays.size())) {
        return false;
    }

    auto& day = fixtureDays[dayNumber - 1];
    if (day.simulated) {
        return false;
    }

    std::cout << "\n===== " << day.label << " =====\n";

    // Set simulation speed for this day's matches
    matchEngine->SetSimulationSpeed(static_cast<SimulationSpeed>(speedMultiplier));

    // First, simulate all non-managed games (or all games if not interactive)
    std::optional<Game> managedGameOpt;
    for (auto& game : day.games) {
        if (game.played) continue;

        bool isManagedGame = false;
        if (managedTeam && (game.homeTeam == managedTeam || game.awayTeam == managedTeam)) {
            isManagedGame = true;
        }

        if (isManagedGame && interactiveManagedGame) {
            // Defer interactive managed game's simulation until after AI games
            managedGameOpt = game;
            continue;
        }

        // Auto-simulate this game
        matchEngine->SetPauseEnabled(false);
        matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
        matchEngine->SimulateFullMatch();

        auto state = matchEngine->GetMatchState();
        game.homeScore = state->homeScore;
        game.awayScore = state->awayScore;
        game.played = true;
        UpdateStandings(game);

        gamesPlayed++;
        std::cout << "[Day " << dayNumber << "] " << game.homeTeam->name << " " << game.homeScore
                  << " - " << game.awayScore << " " << game.awayTeam->name << "\n";
    }

    // If there is an interactive managed game, run it now with pause enabled
    if (managedGameOpt.has_value()) {
        auto game = managedGameOpt.value();

        std::cout << "Your team has a game today: " << game.homeTeam->name << " vs " << game.awayTeam->name << "\n";

        matchEngine->SetPauseEnabled(true);
        matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
        matchEngine->SimulateFullMatch();

        auto state = matchEngine->GetMatchState();
        // find and update the corresponding game in the day.games vector
        for (auto& gref : day.games) {
            if (gref.homeTeam == game.homeTeam && gref.awayTeam == game.awayTeam && !gref.played) {
                gref.homeScore = state->homeScore;
                gref.awayScore = state->awayScore;
                gref.played = true;
                UpdateStandings(gref);
                gamesPlayed++;
                std::cout << "[Day " << dayNumber << "] " << gref.homeTeam->name << " " << gref.homeScore
                          << " - " << gref.awayScore << " " << gref.awayTeam->name << "\n";
                break;
            }
        }
    }

    day.simulated = true;
    CalculateWinPercentages();
    return true;
}

void SeasonManager::SimulateFullSeason(int speedMultiplier) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          SIMULATING " << leagueType << " " << seasonYear << " SEASON          ║\n";
    std::cout << "║              " << totalGames << " games at " << speedMultiplier << "x speed              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    matchEngine->SetSimulationSpeed(static_cast<SimulationSpeed>(speedMultiplier));
    
    int gameNumber = 1;
    for (auto& game : schedule) {
        if (game.played) continue;
        
        // Simulate game
        matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
        matchEngine->SimulateFullMatch();
        
        auto state = matchEngine->GetMatchState();
        game.homeScore = state->homeScore;
        game.awayScore = state->awayScore;
        game.played = true;
        
        // Update standings
        UpdateStandings(game);
        
        // Print progress every 50 games
        if (gameNumber % 50 == 0 || gameNumber == totalGames) {
            std::cout << "[Progress] " << gameNumber << "/" << totalGames << " games complete ("
                      << std::fixed << std::setprecision(1) 
                      << (100.0f * gameNumber / totalGames) << "%)\n";
        }
        
        gamesPlayed++;
        gameNumber++;
    }
    
    CalculateWinPercentages();
    
    std::cout << "\n✅ Regular season complete!\n";
    PrintStandings();
}

void SeasonManager::SimulateNextGame() {
    for (auto& game : schedule) {
        if (!game.played) {
            matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
            matchEngine->SimulateFullMatch();
            
            auto state = matchEngine->GetMatchState();
            game.homeScore = state->homeScore;
            game.awayScore = state->awayScore;
            game.played = true;
            
            UpdateStandings(game);
            gamesPlayed++;
            
            std::cout << "[Game " << gamesPlayed << "/" << totalGames << "] "
                      << game.homeTeam->name << " " << game.homeScore << " - "
                      << game.awayScore << " " << game.awayTeam->name << "\n";
            break;
        }
    }
}

void SeasonManager::UpdateStandings(const Game& game) {
    auto& homeRecord = standings[game.homeTeam->teamId];
    auto& awayRecord = standings[game.awayTeam->teamId];
    
    homeRecord.pointsFor += game.homeScore;
    homeRecord.pointsAgainst += game.awayScore;
    awayRecord.pointsFor += game.awayScore;
    awayRecord.pointsAgainst += game.homeScore;
    
    bool sameConference = (game.homeTeam->conferenceId == game.awayTeam->conferenceId);
    
    if (game.homeScore > game.awayScore) {
        // Home win
        homeRecord.wins++;
        homeRecord.homeWins++;
        awayRecord.losses++;
        awayRecord.awayLosses++;
        
        if (sameConference) {
            homeRecord.conferenceWins++;
            awayRecord.conferenceLosses++;
        }
    } else {
        // Away win
        awayRecord.wins++;
        awayRecord.awayWins++;
        homeRecord.losses++;
        homeRecord.homeLosses++;
        
        if (sameConference) {
            awayRecord.conferenceWins++;
            homeRecord.conferenceLosses++;
        }
    }
}

void SeasonManager::CalculateWinPercentages() {
    for (auto& [teamId, record] : standings) {
        int totalGames = record.wins + record.losses;
        record.winPercentage = totalGames > 0 ? static_cast<float>(record.wins) / totalGames : 0.0f;
    }
}

std::vector<SeasonManager::TeamRecord> SeasonManager::GetStandings(const std::string& conference) const {
    std::vector<TeamRecord> records;
    
    for (const auto& [teamId, record] : standings) {
        if (conference.empty() || record.team->conferenceId == conference) {
            records.push_back(record);
        }
    }
    
    // Sort by win percentage
    std::sort(records.begin(), records.end(), [](const TeamRecord& a, const TeamRecord& b) {
        if (std::abs(a.winPercentage - b.winPercentage) < 0.001f) {
            // Tiebreaker: points differential
            int aDiff = a.pointsFor - a.pointsAgainst;
            int bDiff = b.pointsFor - b.pointsAgainst;
            return aDiff > bDiff;
        }
        return a.winPercentage > b.winPercentage;
    });
    
    return records;
}

void SeasonManager::PrintStandings() const {
    if (leagueType == "NBA") {
        // Print Eastern and Western Conference
        auto eastern = GetStandings("Eastern");
        auto western = GetStandings("Western");
        
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "                 EASTERN CONFERENCE\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "Rank  Team                        W-L      PCT    PF    PA\n";
        std::cout << "───────────────────────────────────────────────────────────\n";
        
        int rank = 1;
        for (const auto& record : eastern) {
            std::cout << std::setw(2) << rank++ << ".   "
                      << std::left << std::setw(25) << record.team->name
                      << std::right << std::setw(2) << record.wins << "-"
                      << std::left << std::setw(2) << record.losses
                      << std::right << std::setw(7) << std::fixed << std::setprecision(3) << record.winPercentage
                      << std::setw(7) << record.pointsFor
                      << std::setw(7) << record.pointsAgainst << "\n";
        }
        
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "                 WESTERN CONFERENCE\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "Rank  Team                        W-L      PCT    PF    PA\n";
        std::cout << "───────────────────────────────────────────────────────────\n";
        
        rank = 1;
        for (const auto& record : western) {
            std::cout << std::setw(2) << rank++ << ".   "
                      << std::left << std::setw(25) << record.team->name
                      << std::right << std::setw(2) << record.wins << "-"
                      << std::left << std::setw(2) << record.losses
                      << std::right << std::setw(7) << std::fixed << std::setprecision(3) << record.winPercentage
                      << std::setw(7) << record.pointsFor
                      << std::setw(7) << record.pointsAgainst << "\n";
        }
    } else {
        // NCAA: show all teams
        auto allStandings = GetStandings();
        
        std::cout << "\n═══════════════════════════════════════════════════════════\n";
        std::cout << "                 " << leagueType << " STANDINGS\n";
        std::cout << "═══════════════════════════════════════════════════════════\n";
        std::cout << "Rank  Team                        W-L      PCT    PF    PA\n";
        std::cout << "───────────────────────────────────────────────────────────\n";
        
        int rank = 1;
        for (const auto& record : allStandings) {
            std::cout << std::setw(2) << rank++ << ".   "
                      << std::left << std::setw(25) << record.team->name
                      << std::right << std::setw(2) << record.wins << "-"
                      << std::left << std::setw(2) << record.losses
                      << std::right << std::setw(7) << std::fixed << std::setprecision(3) << record.winPercentage
                      << std::setw(7) << record.pointsFor
                      << std::setw(7) << record.pointsAgainst << "\n";
        }
    }
    std::cout << "═══════════════════════════════════════════════════════════\n\n";
}

void SeasonManager::GeneratePlayoffBracket() {
    std::cout << "\n[SeasonManager] Generating playoff bracket...\n";
    
    playoffBracket.clear();
    
    if (leagueType == "NBA") {
        // Top 8 from each conference
        auto eastern = GetStandings("Eastern");
        auto western = GetStandings("Western");
        
        // Eastern Conference bracket (1v8, 2v7, 3v6, 4v5)
        for (int i = 0; i < 4; ++i) {
            if (i < eastern.size() && (7-i) < eastern.size()) {
                PlayoffSeries series;
                series.team1 = eastern[i].team;
                series.team2 = eastern[7-i].team;
                playoffBracket.push_back(series);
            }
        }
        
        // Western Conference bracket
        for (int i = 0; i < 4; ++i) {
            if (i < western.size() && (7-i) < western.size()) {
                PlayoffSeries series;
                series.team1 = western[i].team;
                series.team2 = western[7-i].team;
                playoffBracket.push_back(series);
            }
        }
    } else {
        // NCAA: Top 16 teams overall
        auto standings = GetStandings();
        for (int i = 0; i < 8; ++i) {
            if (i < standings.size() && (15-i) < standings.size()) {
                PlayoffSeries series;
                series.team1 = standings[i].team;
                series.team2 = standings[15-i].team;
                playoffBracket.push_back(series);
            }
        }
    }
    
    std::cout << "[SeasonManager] Generated " << playoffBracket.size() << " first-round series\n";
}

void SeasonManager::SimulatePlayoffs(int speedMultiplier) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    PLAYOFFS STARTING                           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    matchEngine->SetSimulationSpeed(static_cast<SimulationSpeed>(speedMultiplier));
    
    int round = 1;
    std::vector<PlayoffSeries> currentRound = playoffBracket;
    
    while (!currentRound.empty()) {
        std::cout << "\n━━━━━━━━━━━━━━━━ ROUND " << round << " ━━━━━━━━━━━━━━━━\n\n";
        
        std::vector<PlayoffSeries> nextRound;
        
        for (auto& series : currentRound) {
            SimulateSeries(series, speedMultiplier);
            
            std::cout << "✓ " << series.winner->name << " defeats "
                      << (series.winner == series.team1 ? series.team2->name : series.team1->name)
                      << " " << series.team1Wins << "-" << series.team2Wins << "\n";
            
            // Winner advances
            if (nextRound.empty() || nextRound.back().team1) {
                PlayoffSeries nextSeries;
                nextSeries.team1 = series.winner;
                nextRound.push_back(nextSeries);
            } else {
                nextRound.back().team2 = series.winner;
            }
        }
        
        // Check if we have a champion
        if (nextRound.size() == 1 && nextRound[0].team2 == nullptr) {
            std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
            std::cout << "║                🏆 CHAMPIONS 🏆                                 ║\n";
            std::cout << "║           " << std::left << std::setw(48) << nextRound[0].team1->name << "   ║\n";
            std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
            playoffsComplete = true;
            break;
        }
        
        currentRound = nextRound;
        round++;
    }
}

void SeasonManager::SimulateSeries(PlayoffSeries& series, int speedMultiplier) {
    // Best of 7 series
    while (series.team1Wins < 4 && series.team2Wins < 4) {
        Game game;
        // Alternate home court
        int gameNumber = series.team1Wins + series.team2Wins;
        bool team1Home = (gameNumber % 2 == 0);
        
        game.homeTeam = team1Home ? series.team1 : series.team2;
        game.awayTeam = team1Home ? series.team2 : series.team1;
        
        matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
        matchEngine->SimulateFullMatch();
        
        auto state = matchEngine->GetMatchState();
        game.homeScore = state->homeScore;
        game.awayScore = state->awayScore;
        game.played = true;
        
        series.games.push_back(game);
        
        // Update series wins
        if (game.homeScore > game.awayScore) {
            if (team1Home) series.team1Wins++;
            else series.team2Wins++;
        } else {
            if (team1Home) series.team2Wins++;
            else series.team1Wins++;
        }
    }
    
    series.complete = true;
    series.winner = (series.team1Wins == 4) ? series.team1 : series.team2;
}

} // namespace engine
} // namespace bm
