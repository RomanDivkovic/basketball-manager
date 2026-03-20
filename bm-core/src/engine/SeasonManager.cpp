#include "engine/SeasonManager.h"
#include "engine/MatchEngine.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <random>

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
    // NBA: 82 games per team
    // Simple round-robin: each team plays every other team multiple times
    // For 30 teams: play each team ~2-3 times (82 games total per team)
    
    int gamesPerMatchup = 3; // Each pair plays 3 times
    
    for (size_t i = 0; i < allTeams.size(); ++i) {
        for (size_t j = i + 1; j < allTeams.size(); ++j) {
            for (int game = 0; game < gamesPerMatchup; ++game) {
                Game g;
                // Alternate home/away
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
    
    // Shuffle schedule for realistic ordering
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(schedule.begin(), schedule.end(), gen);
}

void SeasonManager::GenerateNCAASchedule() {
    // NCAA: ~30 games per team (conference play focused)
    // Simpler schedule: play each team 1-2 times
    
    int gamesPerMatchup = 2;
    
    for (size_t i = 0; i < allTeams.size(); ++i) {
        for (size_t j = i + 1; j < allTeams.size(); ++j) {
            // Check if same conference (play more often)
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
    
    // Shuffle
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(schedule.begin(), schedule.end(), gen);
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
