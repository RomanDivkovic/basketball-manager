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
    : seasonYear(2025), leagueType("NBA"), gamesPlayed(0), totalGames(0),
      playoffsGenerated(false), playoffsComplete(false), currentPlayoffRound(0), champion(nullptr) {
    matchEngine = std::make_shared<MatchEngine>();
    std::cout << "[SeasonManager] Initialized\n";
}

SeasonManager::~SeasonManager() {
}

void SeasonManager::InitializeSeason(int year, const std::string& league) {
    seasonYear = year;
    leagueType = league;
    gamesPlayed = 0;
    playoffsGenerated = false;
    playoffsComplete = false;
    currentPlayoffRound = 0;
    champion = nullptr;
    schedule.clear();
    fixtureDays.clear();
    standings.clear();
    playoffBracket.clear();
    
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
    int maxAttempts = 50000;
    while (maxAttempts-- > 0) {
        bool done = true;
        for (int t = 0; t < totalTeams; ++t) {
            if (gamesRemaining[t] > 0) {
                done = false;
                break;
            }
        }
        if (done) break;

        bool madeProgress = false;
        for (int i = 0; i < totalTeams; ++i) {
            if (gamesRemaining[i] <= 0) continue;
            for (int j = 0; j < totalTeams; ++j) {
                if (i == j || gamesRemaining[j] <= 0) continue;
                auto pairKey = std::make_pair(std::min(i, j), std::max(i, j));
                if (pairCounts[pairKey] >= 4) continue;

                Game g;
                if ((pairCounts[pairKey] + 1) % 2 == 0) {
                    g.homeTeam = allTeams[i];
                    g.awayTeam = allTeams[j];
                } else {
                    g.homeTeam = allTeams[j];
                    g.awayTeam = allTeams[i];
                }
                schedule.push_back(g);
                pairCounts[pairKey]++;
                gamesRemaining[i]--;
                gamesRemaining[j]--;
                madeProgress = true;

                if (gamesRemaining[i] <= 0) {
                    break;
                }
            }
        }
        if (!madeProgress) {
            // Break if no valid pairs remaining under max constraints
            break;
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

const SeasonManager::FixtureDay* SeasonManager::GetFixtureDay(int dayNumber) const {
    const int index = std::max(0, dayNumber - 1);
    if (index >= static_cast<int>(fixtureDays.size())) {
        return nullptr;
    }
    return &fixtureDays[index];
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
    std::cout << "\n[SeasonManager] Generating NBA Play-In & Playoff bracket...\n";
    
    playoffBracket.clear();
    currentPlayoffRound = 0;
    playoffsGenerated = true;
    playoffsComplete = false;
    champion = nullptr;
    
    if (leagueType == "NBA") {
        auto eastern = GetStandings("Eastern");
        auto western = GetStandings("Western");

        // Ensure we have enough teams
        if (eastern.size() >= 10 && western.size() >= 10) {
            // Round 0: Play-In Tournament
            // Game 1: 7 vs 8 (winner gets Seed 7)
            // Game 2: 9 vs 10 (loser eliminated)
            // Game 3: Loser of 7v8 vs Winner of 9v10 (winner gets Seed 8)
            auto createPlayIn = [&](const std::string& conf, const auto& confStandings) {
                PlayoffSeries pi1; // 7 vs 8
                pi1.seriesId = conf + "_PI_7v8";
                pi1.roundName = "Play-In (7v8)";
                pi1.conference = conf;
                pi1.roundIndex = 0;
                pi1.seriesIndex = 0;
                pi1.team1 = confStandings[6].team; // Seed 7
                pi1.team2 = confStandings[7].team; // Seed 8
                pi1.team1Seed = 7;
                pi1.team2Seed = 8;
                pi1.targetWins = 1;
                playoffBracket.push_back(pi1);

                PlayoffSeries pi2; // 9 vs 10
                pi2.seriesId = conf + "_PI_9v10";
                pi2.roundName = "Play-In (9v10)";
                pi2.conference = conf;
                pi2.roundIndex = 0;
                pi2.seriesIndex = 1;
                pi2.team1 = confStandings[8].team; // Seed 9
                pi2.team2 = confStandings[9].team; // Seed 10
                pi2.team1Seed = 9;
                pi2.team2Seed = 10;
                pi2.targetWins = 1;
                playoffBracket.push_back(pi2);
            };

            createPlayIn("Eastern", eastern);
            createPlayIn("Western", western);
        } else {
            // Fallback: direct 1-8 playoff bracket if fewer than 10 teams per conf
            currentPlayoffRound = 1;
            auto createConfQuarters = [&](const std::string& conf, const auto& confStandings) {
                int pairings[4][2] = {{0, 7}, {3, 4}, {2, 5}, {1, 6}}; // 1v8, 4v5, 3v6, 2v7
                for (int i = 0; i < 4; ++i) {
                    if (pairings[i][0] < confStandings.size() && pairings[i][1] < confStandings.size()) {
                        PlayoffSeries s;
                        s.seriesId = conf + "_R1_" + std::to_string(i + 1);
                        s.roundName = "Conference First Round";
                        s.conference = conf;
                        s.roundIndex = 1;
                        s.seriesIndex = i;
                        s.team1 = confStandings[pairings[i][0]].team;
                        s.team2 = confStandings[pairings[i][1]].team;
                        s.team1Seed = pairings[i][0] + 1;
                        s.team2Seed = pairings[i][1] + 1;
                        s.targetWins = 4;
                        playoffBracket.push_back(s);
                    }
                }
            };
            createConfQuarters("Eastern", eastern);
            createConfQuarters("Western", western);
        }
    } else {
        // NCAA: Top 16 single elimination or best-of-7
        auto standingsList = GetStandings();
        currentPlayoffRound = 1;
        for (int i = 0; i < 8; ++i) {
            if (i < standingsList.size() && (15 - i) < standingsList.size()) {
                PlayoffSeries s;
                s.seriesId = "NCAA_R1_" + std::to_string(i + 1);
                s.roundName = "First Round";
                s.conference = "National";
                s.roundIndex = 1;
                s.seriesIndex = i;
                s.team1 = standingsList[i].team;
                s.team2 = standingsList[15 - i].team;
                s.team1Seed = i + 1;
                s.team2Seed = 16 - i;
                s.targetWins = 4;
                playoffBracket.push_back(s);
            }
        }
    }
    
    std::cout << "[SeasonManager] Generated playoff bracket with " << playoffBracket.size() << " initial series\n";
}

bool SeasonManager::SimulatePlayoffGame(const std::string& seriesId, int speedMultiplier) {
    for (auto& series : playoffBracket) {
        if (series.seriesId == seriesId) {
            if (series.complete || !series.team1 || !series.team2) return false;

            if (matchEngine) {
                matchEngine->SetSpeedMultiplier(speedMultiplier >= 6 ? 0 : speedMultiplier);
            }

            int gameNum = series.team1Wins + series.team2Wins;
            bool team1Home = true;
            if (series.targetWins == 4) {
                // NBA 2-2-1-1-1 format: Games 1,2,5,7 at team1, 3,4,6 at team2
                team1Home = (gameNum == 0 || gameNum == 1 || gameNum == 4 || gameNum == 6);
            }

            Game game;
            game.homeTeam = team1Home ? series.team1 : series.team2;
            game.awayTeam = team1Home ? series.team2 : series.team1;

            matchEngine->InitializeMatch(game.homeTeam, game.awayTeam);
            matchEngine->SimulateFullMatch();

            auto state = matchEngine->GetMatchState();
            game.homeScore = state->homeScore;
            game.awayScore = state->awayScore;
            game.played = true;

            series.games.push_back(game);

            if (game.homeScore > game.awayScore) {
                if (team1Home) series.team1Wins++;
                else series.team2Wins++;
            } else {
                if (team1Home) series.team2Wins++;
                else series.team1Wins++;
            }

            if (series.team1Wins >= series.targetWins) {
                series.complete = true;
                series.winner = series.team1;
            } else if (series.team2Wins >= series.targetWins) {
                series.complete = true;
                series.winner = series.team2;
            }

            // Check if all series in this round are complete; if so, setup next round
            bool allRoundComplete = true;
            for (const auto& s : playoffBracket) {
                if (s.roundIndex == currentPlayoffRound && !s.complete) {
                    allRoundComplete = false;
                    break;
                }
            }
            if (allRoundComplete) {
                SetupNextPlayoffRound();
            }

            return true;
        }
    }
    return false;
}

bool SeasonManager::AdvancePlayoffRound(int speedMultiplier) {
    if (!playoffsGenerated || playoffsComplete) return false;

    // Simulate all remaining incomplete series in the current round
    bool simulatedAny = false;
    for (auto& series : playoffBracket) {
        if (series.roundIndex == currentPlayoffRound && !series.complete && series.team1 && series.team2) {
            SimulateSeries(series, speedMultiplier);
            simulatedAny = true;
        }
    }

    SetupNextPlayoffRound();
    return true;
}

void SeasonManager::SetupNextPlayoffRound() {
    if (playoffsComplete) return;

    if (currentPlayoffRound == 0) {
        // Play-In Initial matches complete. Check if final 8th seed match needs to be added,
        // or if we transition to Round 1 (Conference First Round).
        // Let's check for final play-in games
        auto findSeries = [&](const std::string& id) -> PlayoffSeries* {
            for (auto& s : playoffBracket) {
                if (s.seriesId == id) return &s;
            }
            return nullptr;
        };

        bool hasFinalEastPI = findSeries("Eastern_PI_Final") != nullptr;
        bool hasFinalWestPI = findSeries("Western_PI_Final") != nullptr;

        if (!hasFinalEastPI && !hasFinalWestPI) {
            auto* e7v8 = findSeries("Eastern_PI_7v8");
            auto* e9v10 = findSeries("Eastern_PI_9v10");
            auto* w7v8 = findSeries("Western_PI_7v8");
            auto* w9v10 = findSeries("Western_PI_9v10");

            if (e7v8 && e7v8->complete && e9v10 && e9v10->complete &&
                w7v8 && w7v8->complete && w9v10 && w9v10->complete) {
                // Loser of 7v8 vs Winner of 9v10
                auto getLoser = [](PlayoffSeries* s) {
                    return (s->winner == s->team1) ? s->team2 : s->team1;
                };

                PlayoffSeries eFinal;
                eFinal.seriesId = "Eastern_PI_Final";
                eFinal.roundName = "Play-In (8th Seed Decider)";
                eFinal.conference = "Eastern";
                eFinal.roundIndex = 0;
                eFinal.seriesIndex = 2;
                eFinal.team1 = getLoser(e7v8);
                eFinal.team2 = e9v10->winner;
                eFinal.team1Seed = 8;
                eFinal.team2Seed = 9;
                eFinal.targetWins = 1;
                playoffBracket.push_back(eFinal);

                PlayoffSeries wFinal;
                wFinal.seriesId = "Western_PI_Final";
                wFinal.roundName = "Play-In (8th Seed Decider)";
                wFinal.conference = "Western";
                wFinal.roundIndex = 0;
                wFinal.seriesIndex = 2;
                wFinal.team1 = getLoser(w7v8);
                wFinal.team2 = w9v10->winner;
                wFinal.team1Seed = 8;
                wFinal.team2Seed = 9;
                wFinal.targetWins = 1;
                playoffBracket.push_back(wFinal);
                return;
            }
        }

        // If finals exist and are complete, construct Round 1 (Conference Quarterfinals)
        auto* eFinal = findSeries("Eastern_PI_Final");
        auto* wFinal = findSeries("Western_PI_Final");
        if (eFinal && eFinal->complete && wFinal && wFinal->complete) {
            auto eastern = GetStandings("Eastern");
            auto western = GetStandings("Western");
            auto* e7v8 = findSeries("Eastern_PI_7v8");
            auto* w7v8 = findSeries("Western_PI_7v8");

            auto eSeed7 = e7v8->winner;
            auto eSeed8 = eFinal->winner;
            auto wSeed7 = w7v8->winner;
            auto wSeed8 = wFinal->winner;

            currentPlayoffRound = 1;

            auto setupRound1Conf = [&](const std::string& conf, const auto& confStandings,
                                       const std::shared_ptr<Team>& s7, const std::shared_ptr<Team>& s8) {
                std::vector<std::pair<std::shared_ptr<Team>, int>> seeds = {
                    {confStandings[0].team, 1},
                    {confStandings[1].team, 2},
                    {confStandings[2].team, 3},
                    {confStandings[3].team, 4},
                    {confStandings[4].team, 5},
                    {confStandings[5].team, 6},
                    {s7, 7},
                    {s8, 8}
                };

                // Standard NBA bracket matchups: 1v8, 4v5, 3v6, 2v7
                int matchups[4][2] = {{0, 7}, {3, 4}, {2, 5}, {1, 6}};
                for (int i = 0; i < 4; ++i) {
                    PlayoffSeries s;
                    s.seriesId = conf + "_R1_" + std::to_string(i + 1);
                    s.roundName = "Conference First Round";
                    s.conference = conf;
                    s.roundIndex = 1;
                    s.seriesIndex = i;
                    s.team1 = seeds[matchups[i][0]].first;
                    s.team2 = seeds[matchups[i][1]].first;
                    s.team1Seed = seeds[matchups[i][0]].second;
                    s.team2Seed = seeds[matchups[i][1]].second;
                    s.targetWins = 4;
                    playoffBracket.push_back(s);
                }
            };

            setupRound1Conf("Eastern", eastern, eSeed7, eSeed8);
            setupRound1Conf("Western", western, wSeed7, wSeed8);
            std::cout << "[SeasonManager] Round 1 (Conference First Round) created!\n";
        }
    } else if (currentPlayoffRound == 1) {
        // Round 1 -> Round 2 (Conference Semifinals)
        // 1v8 winner plays 4v5 winner; 2v7 winner plays 3v6 winner
        auto createConfSemis = [&](const std::string& conf) {
            auto getR1Winner = [&](int seriesNum) -> std::shared_ptr<Team> {
                for (const auto& s : playoffBracket) {
                    if (s.seriesId == conf + "_R1_" + std::to_string(seriesNum)) return s.winner;
                }
                return nullptr;
            };

            auto w1v8 = getR1Winner(1);
            auto w4v5 = getR1Winner(2);
            auto w3v6 = getR1Winner(3);
            auto w2v7 = getR1Winner(4);

            if (w1v8 && w4v5 && w3v6 && w2v7) {
                PlayoffSeries semi1;
                semi1.seriesId = conf + "_R2_1";
                semi1.roundName = "Conference Semifinals";
                semi1.conference = conf;
                semi1.roundIndex = 2;
                semi1.seriesIndex = 0;
                semi1.team1 = w1v8;
                semi1.team2 = w4v5;
                semi1.targetWins = 4;
                playoffBracket.push_back(semi1);

                PlayoffSeries semi2;
                semi2.seriesId = conf + "_R2_2";
                semi2.roundName = "Conference Semifinals";
                semi2.conference = conf;
                semi2.roundIndex = 2;
                semi2.seriesIndex = 1;
                semi2.team1 = w2v7;
                semi2.team2 = w3v6;
                semi2.targetWins = 4;
                playoffBracket.push_back(semi2);
            }
        };

        createConfSemis("Eastern");
        createConfSemis("Western");
        currentPlayoffRound = 2;
        std::cout << "[SeasonManager] Round 2 (Conference Semifinals) created!\n";
    } else if (currentPlayoffRound == 2) {
        // Round 2 -> Round 3 (Conference Finals)
        auto createConfFinals = [&](const std::string& conf) {
            std::shared_ptr<Team> wSemi1 = nullptr;
            std::shared_ptr<Team> wSemi2 = nullptr;
            for (const auto& s : playoffBracket) {
                if (s.seriesId == conf + "_R2_1") wSemi1 = s.winner;
                if (s.seriesId == conf + "_R2_2") wSemi2 = s.winner;
            }

            if (wSemi1 && wSemi2) {
                PlayoffSeries confFinal;
                confFinal.seriesId = conf + "_R3";
                confFinal.roundName = "Conference Finals";
                confFinal.conference = conf;
                confFinal.roundIndex = 3;
                confFinal.seriesIndex = 0;
                confFinal.team1 = wSemi1;
                confFinal.team2 = wSemi2;
                confFinal.targetWins = 4;
                playoffBracket.push_back(confFinal);
            }
        };

        createConfFinals("Eastern");
        createConfFinals("Western");
        currentPlayoffRound = 3;
        std::cout << "[SeasonManager] Round 3 (Conference Finals) created!\n";
    } else if (currentPlayoffRound == 3) {
        // Round 3 -> Round 4 (NBA Finals)
        std::shared_ptr<Team> eastChamp = nullptr;
        std::shared_ptr<Team> westChamp = nullptr;
        for (const auto& s : playoffBracket) {
            if (s.seriesId == "Eastern_R3") eastChamp = s.winner;
            if (s.seriesId == "Western_R3") westChamp = s.winner;
        }

        if (eastChamp && westChamp) {
            PlayoffSeries finals;
            finals.seriesId = "NBA_FINALS";
            finals.roundName = "NBA Finals";
            finals.conference = "NBA Finals";
            finals.roundIndex = 4;
            finals.seriesIndex = 0;

            // Determine home court by regular season win percentage
            auto eRec = standings[eastChamp->teamId];
            auto wRec = standings[westChamp->teamId];
            if (eRec.winPercentage >= wRec.winPercentage) {
                finals.team1 = eastChamp;
                finals.team2 = westChamp;
            } else {
                finals.team1 = westChamp;
                finals.team2 = eastChamp;
            }
            finals.targetWins = 4;
            playoffBracket.push_back(finals);

            currentPlayoffRound = 4;
            std::cout << "[SeasonManager] NBA Finals matchup set: " << finals.team1->name << " vs " << finals.team2->name << "!\n";
        }
    } else if (currentPlayoffRound == 4) {
        // NBA Finals complete
        for (const auto& s : playoffBracket) {
            if (s.seriesId == "NBA_FINALS" && s.complete) {
                champion = s.winner;
                playoffsComplete = true;
                std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
                std::cout << "║            🏆 2026/27 NBA WORLD CHAMPIONS 🏆                   ║\n";
                std::cout << "║               " << std::left << std::setw(48) << champion->name << " ║\n";
                std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
                break;
            }
        }
    }
}

void SeasonManager::SimulatePlayoffs(int speedMultiplier) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    PLAYOFFS STARTING                           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

    if (!playoffsGenerated) {
        GeneratePlayoffBracket();
    }

    while (!playoffsComplete) {
        AdvancePlayoffRound(speedMultiplier);
    }
}

void SeasonManager::SimulateSeries(PlayoffSeries& series, int speedMultiplier) {
    if (matchEngine) {
        matchEngine->SetSpeedMultiplier(speedMultiplier >= 6 ? 0 : speedMultiplier);
    }
    int target = series.targetWins > 0 ? series.targetWins : 4;
    while (series.team1Wins < target && series.team2Wins < target) {
        Game game;
        int gameNumber = series.team1Wins + series.team2Wins;
        bool team1Home = true;
        if (target == 4) {
            team1Home = (gameNumber == 0 || gameNumber == 1 || gameNumber == 4 || gameNumber == 6);
        } else {
            team1Home = (gameNumber % 2 == 0);
        }
        
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
    series.winner = (series.team1Wins >= target) ? series.team1 : series.team2;
}

} // namespace engine
} // namespace bm
