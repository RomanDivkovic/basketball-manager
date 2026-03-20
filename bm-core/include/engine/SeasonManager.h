#pragma once

#include "common/Types.h"
#include <memory>
#include <vector>
#include <map>
#include <string>

namespace bm {
namespace engine {

class MatchEngine;
using bm::Team;

/**
 * SeasonManager - Manages full season simulation
 * Handles schedule generation, standings tracking, and playoffs
 */
class SeasonManager {
public:
    SeasonManager();
    ~SeasonManager();

    // Season setup
    void InitializeSeason(int year, const std::string& leagueType); // "NBA" or "NCAA"
    void LoadTeams(const std::vector<std::shared_ptr<Team>>& teams);
    
    // Schedule generation
    void GenerateSchedule();
    
    // Season simulation
    void SimulateFullSeason(int speedMultiplier = 6);
    void SimulateUntilDate(int gamesPlayed);
    void SimulateNextGame();
    
    // Standings
    struct TeamRecord {
        std::shared_ptr<Team> team;
        int wins;
        int losses;
        int homeWins;
        int homeLosses;
        int awayWins;
        int awayLosses;
        int pointsFor;
        int pointsAgainst;
        float winPercentage;
        int conferenceWins;
        int conferenceLosses;
        int divisionWins;
        int divisionLosses;
        
        TeamRecord() : wins(0), losses(0), homeWins(0), homeLosses(0),
                      awayWins(0), awayLosses(0), pointsFor(0), pointsAgainst(0),
                      winPercentage(0.0f), conferenceWins(0), conferenceLosses(0),
                      divisionWins(0), divisionLosses(0) {}
    };
    
    std::vector<TeamRecord> GetStandings(const std::string& conference = "") const;
    void PrintStandings() const;
    
    // Playoffs
    void GeneratePlayoffBracket();
    void SimulatePlayoffs(int speedMultiplier = 6);
    
    // Season info
    int GetGamesPlayed() const { return gamesPlayed; }
    int GetTotalGames() const { return totalGames; }
    bool IsSeasonComplete() const { return gamesPlayed >= totalGames; }
    bool ArePlayoffsComplete() const { return playoffsComplete; }
    
private:
    struct Game {
        std::shared_ptr<Team> homeTeam;
        std::shared_ptr<Team> awayTeam;
        int homeScore;
        int awayScore;
        bool played;
        
        Game() : homeScore(0), awayScore(0), played(false) {}
    };
    
    std::shared_ptr<MatchEngine> matchEngine;
    std::vector<std::shared_ptr<Team>> allTeams;
    std::map<std::string, TeamRecord> standings;
    std::vector<Game> schedule;
    
    int seasonYear;
    std::string leagueType;
    int gamesPlayed;
    int totalGames;
    bool playoffsComplete;
    
    // Schedule generation helpers
    void GenerateNBASchedule();  // 82 games per team
    void GenerateNCAASchedule(); // 30 games per team
    
    // Standings helpers
    void UpdateStandings(const Game& game);
    void CalculateWinPercentages();
    
    // Playoff helpers
    struct PlayoffSeries {
        std::shared_ptr<Team> team1;
        std::shared_ptr<Team> team2;
        int team1Wins;
        int team2Wins;
        std::vector<Game> games;
        bool complete;
        std::shared_ptr<Team> winner;
        
        PlayoffSeries() : team1Wins(0), team2Wins(0), complete(false) {}
    };
    
    std::vector<PlayoffSeries> playoffBracket;
    void SimulateSeries(PlayoffSeries& series, int speedMultiplier);
};

} // namespace engine
} // namespace bm
