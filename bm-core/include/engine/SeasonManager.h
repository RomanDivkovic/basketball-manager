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

    struct Game {
        std::shared_ptr<Team> homeTeam;
        std::shared_ptr<Team> awayTeam;
        int homeScore;
        int awayScore;
        bool played;

        Game() : homeScore(0), awayScore(0), played(false) {}
    };

    struct FixtureDay {
        int dayNumber;
        std::string label;
        std::vector<Game> games;
        bool simulated;

        FixtureDay() : dayNumber(0), simulated(false) {}
    };

    // Season setup
    void InitializeSeason(int year, const std::string& leagueType); // "NBA" or "NCAA"
    void LoadTeams(const std::vector<std::shared_ptr<Team>>& teams);
    
    // Schedule generation
    void GenerateSchedule();
    void GenerateNBASchedule();
    void GenerateNCAASchedule();
    std::vector<Game> GetFixturesForDay(int dayNumber) const;
    int GetFixtureDayCount() const { return static_cast<int>(fixtureDays.size()); }
    // Simulate all games scheduled for a fixture day.
    // - autoSimComputerGames: if true, computer-vs-computer games are auto-simulated.
    // - managedTeam: optional team being managed by the user; used to surface urgency.
    // - interactiveManagedGame: if true and the managedTeam has a game today, that
    //   managed game's simulation will enable interactive pause handling so the user
    //   can control the match while other games are auto-simulated.
    // - speedMultiplier: simulation speed (1,2,3,4,6) forwarded to the match engine.
    bool SimulateDay(int dayNumber,
                     bool autoSimComputerGames = true,
                     const std::shared_ptr<Team>& managedTeam = nullptr,
                     bool interactiveManagedGame = false,
                     int speedMultiplier = 6);
    
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
    std::shared_ptr<MatchEngine> matchEngine;
    std::vector<std::shared_ptr<Team>> allTeams;
    std::map<std::string, TeamRecord> standings;
    std::vector<Game> schedule;
    std::vector<FixtureDay> fixtureDays;

    int seasonYear;
    std::string leagueType;
    int gamesPlayed;
    int totalGames;
    bool playoffsComplete;

    void BuildFixtureDays();
    
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
