#pragma once

#include "common/Types.h"
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
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
    void SetGamesPerTeam(int games) { gamesPerTeam = games; }
    void SetScheduleSeed(unsigned seed) { scheduleSeed = seed; hasScheduleSeed = true; }
    // Adds a finished game's box score of the team's players to their season totals
    void AddPlayerGameStats(const std::shared_ptr<Team>& team, bool playoffs);
    void SetTeamTactics(const std::string& teamId, int offense, int defense) { teamTactics[teamId] = {offense, defense}; }   // from the staff, for games the assistant plays
    void SetTeamLineup(const std::string& teamId, const std::vector<std::string>& starters, const std::unordered_map<std::string, int>& minutes) { teamLineups[teamId] = Lineup{starters, minutes}; }
    // Quiet = headless matches and no console output (used when a whole year is simulated)
    void SetQuiet(bool value);
    int GetSeasonYear() const { return seasonYear; }
    const std::vector<std::shared_ptr<Team>>& GetTeams() const { return allTeams; }

    // Plays one game between any two teams without touching standings (summer league, exhibitions)
    std::pair<int, int> PlayStandaloneGame(const std::shared_ptr<Team>& home, const std::shared_ptr<Team>& away);

    // Days where one team's game is held back so the human coach can play it
    // Returns true when the held team has an unplayed game on that day.
    bool PlayDayExcept(int dayNumber, const std::string& holdTeamId);
    bool HasUnplayedGame(int dayNumber, const std::string& teamId, std::shared_ptr<Team>* home = nullptr, std::shared_ptr<Team>* away = nullptr) const;
    bool RecordResult(int dayNumber, const std::string& homeId, const std::string& awayId, int homeScore, int awayScore);
    bool SimulateHeldGame(int dayNumber, const std::string& teamId);
    
    // Schedule generation
    void GenerateSchedule();
    void GenerateNBASchedule();
    void GenerateNCAASchedule();
    std::vector<Game> GetFixturesForDay(int dayNumber) const;
    const FixtureDay* GetFixtureDay(int dayNumber) const;
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
    struct PlayoffSeries {
        std::string seriesId;
        std::string roundName; // "Play-In", "Conference Quarterfinals", "Conference Semifinals", "Conference Finals", "NBA Finals"
        std::string conference; // "Eastern", "Western", or "NBA Finals"
        int roundIndex; // 0=PlayIn, 1=FirstRound, 2=ConfSemis, 3=ConfFinals, 4=NBAFinals
        int seriesIndex;
        std::shared_ptr<Team> team1; // Higher seed / home court
        std::shared_ptr<Team> team2; // Lower seed
        int team1Seed;
        int team2Seed;
        int team1Wins;
        int team2Wins;
        std::vector<Game> games;
        bool complete;
        std::shared_ptr<Team> winner;
        int targetWins; // 1 for Play-In, 4 for Best-of-7

        PlayoffSeries() : roundIndex(0), seriesIndex(0), team1Seed(0), team2Seed(0),
                          team1Wins(0), team2Wins(0), complete(false), targetWins(4) {}
    };

    void GeneratePlayoffBracket();
    bool AdvancePlayoffRound(int speedMultiplier = 6);
    bool SimulatePlayoffGame(const std::string& seriesId, int speedMultiplier = 6);
    void SimulatePlayoffs(int speedMultiplier = 6);

    // Day-by-day playoffs: who plays the next game of a series, and recording its result
    bool NextPlayoffGameTeams(const std::string& seriesId, std::shared_ptr<Team>* home, std::shared_ptr<Team>* away) const;
    bool RecordPlayoffGame(const std::string& seriesId, int homeScore, int awayScore);
    
    // Playoff inspection
    const std::vector<PlayoffSeries>& GetPlayoffBracket() const { return playoffBracket; }
    int GetCurrentPlayoffRound() const { return currentPlayoffRound; }
    const std::shared_ptr<Team>& GetChampion() const { return champion; }
    bool ArePlayoffsGenerated() const { return playoffsGenerated; }
    bool ArePlayoffsComplete() const { return playoffsComplete; }

    // Season info
    int GetGamesPlayed() const { return gamesPlayed; }
    int GetTotalGames() const { return totalGames; }
    bool IsSeasonComplete() const { return gamesPlayed >= totalGames; }
    
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
    bool playoffsGenerated;
    bool playoffsComplete;
    int currentPlayoffRound;
    std::shared_ptr<Team> champion;
    int gamesPerTeam = 82;
    struct Lineup { std::vector<std::string> starters; std::unordered_map<std::string, int> minutes; };
    std::map<std::string, std::pair<int, int>> teamTactics;
    std::map<std::string, Lineup> teamLineups;   // the coach's choices by team id
    bool recordStats = true;
    unsigned scheduleSeed = 0;
    bool hasScheduleSeed = false;
    bool quiet = false;

    void BuildFixtureDays();
    void PackFixtureDays();
    void SimulateGameInPlace(Game& game);
    
    // Standings helpers
    void UpdateStandings(const Game& game);
    void CalculateWinPercentages();
    
    // Playoff helpers
    std::vector<PlayoffSeries> playoffBracket;
    void SimulateSeries(PlayoffSeries& series, int speedMultiplier);
    void SetupNextPlayoffRound();
};

} // namespace engine
} // namespace bm
