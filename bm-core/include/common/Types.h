#pragma once

#include <string>
#include <vector>
#include <memory>

namespace bm {

/**
 * Common data structures used across the engine
 */

// Position enumeration
enum class Position {
    PG = 0,  // Point Guard
    SG = 1,  // Shooting Guard
    SF = 2,  // Small Forward
    PF = 3,  // Power Forward
    C = 4    // Center
};

// Player attributes (0-20 scale)
struct PlayerAttributes {
    int pace;           // Movement speed
    int shooting;       // Shooting ability
    int ballControl;    // Ball handling
    int defense;        // Defensive ability
    int physical;       // Strength/athleticism
    int technical;      // Basketball IQ
    
    int currentAbility; // Current rating (0-100)
    int potentialAbility; // Future potential (0-100)
};

// Player structure
struct Player {
    std::string playerId;
    std::string firstName;
    std::string lastName;
    
    Position position;
    float height;       // in meters
    float weight;       // in kg
    std::string birthDate;
    int jerseyNumber;
    
    PlayerAttributes attributes;
    
    // NCAA specific
    int yearInSchool;
    bool international;
    bool draftEligible;
    int nilValue;
    
    // Status
    bool active;
    bool injured;
    int suspensionMatches;
    
    // Statistics
    float gamesPlayed;
    float minutesPlayed;
    float pointsPerGame;
    float reboundsPerGame;
    float assistsPerGame;
    
    // Match state (in-game tracking)
    bool onCourt;           // Currently playing
    float minutesPlayedThisGame;  // Accumulated minutes this match
    float fatigueLevel;     // 0.0 (fresh) to 1.0 (exhausted)
    int pointsThisGame;
    int reboundsThisGame;
    int assistsThisGame;
    int stealsThisGame;
    int blocksThisGame;
    int turnoversThisGame;
    int foulsThisGame;
};

// Team structure
struct Team {
    std::string teamId;
    std::string name;
    std::string city;
    std::string state;
    std::string arena;
    
    std::string conferenceId;
    
    int prestige;       // 0-100 scale
    int budget;         // Annual budget
    int nilBudget;      // NIL pool budget
    
    std::vector<std::shared_ptr<Player>> roster;
    
    bool active;
};

// Court player position for 2D/3D visualization (coordinates 0-94 length, 0-50 width)
struct CourtPlayerState {
    std::string playerId;
    std::string name;
    std::string teamId;
    bool isHome;
    Position position;
    int jerseyNumber;
    float x; // Court length: 0.0 to 94.0 ft
    float y; // Court width: 0.0 to 50.0 ft
    bool hasBall;
    std::string action; // "idle", "dribble", "pass", "shoot", "defend", "rebound", "steal", "block", "turnover", "foul"
};

// Ball position in 3D/2D space
struct BallState {
    float x; // 0.0 to 94.0 ft
    float y; // 0.0 to 50.0 ft
    float z; // Height above floor in feet (0 = on floor, 10 = rim height)
    std::string holderPlayerId; // Empty if ball is in flight / loose
    bool inFlight;
    std::string phase; // "dribble", "pass", "shot", "loose", "dead"
};

// Match state
struct MatchState {
    std::shared_ptr<Team> homeTeam;
    std::shared_ptr<Team> awayTeam;
    
    int homeScore;
    int awayScore;
    
    int quarterNumber;      // 1-4, or 5+ for OT
    int timeRemaining;      // In seconds (600 per quarter = 10 min for testing, 720 for real)
    int timeElapsed;        // Total elapsed time in seconds
    
    int homeRebounds;
    int awayRebounds;
    
    int homeFouls;
    int awayFouls;

    int homeSteals;
    int awaySteals;

    int homeBlocks;
    int awayBlocks;

    int homeTurnovers;
    int awayTurnovers;
    
    // Possession tracking
    bool homeTeamPossession;
    bool jumpBallWinner;    // true = home team won Q1 tip-off
    int possessionCount;
    
    std::shared_ptr<Player> lastScorer;
    std::shared_ptr<Player> lastRebounder;
    std::string lastPlay;
    
    bool matchComplete;
    
    // Starting lineups
    std::vector<std::shared_ptr<Player>> homeStartingFive;
    std::vector<std::shared_ptr<Player>> awayStartingFive;

    // Spatial visualization state (10 active players + ball)
    std::vector<CourtPlayerState> courtPlayers;
    BallState ball;
};

// Tactic configuration
struct Tactic {
    std::string name;
    std::string offensiveSet;  // "pick_and_roll", "isolation", etc.
    std::string defensiveScheme; // "man", "zone_2_3", etc.
    
    std::vector<std::shared_ptr<Player>> startingFive;
    
    float aggressiveness; // 0.0 - 1.0
};

// Contract structure
struct Contract {
    std::string contractId;
    std::string playerId;
    std::string teamId;
    
    // NCAA: Scholarship
    bool scholarshipOffer;
    float scholarshipPercent;
    
    // Professional
    int startYear;
    int endYear;
    int annualSalary;
    int totalGuaranteed;
    
    std::string status; // "active", "expired", etc.
};

// Statistics structure
struct StatLine {
    int gamesPlayed;
    float minutes;
    
    int pointsFor;
    int pointsAgainst;
    
    int fieldGoalsMade;
    int fieldGoalsAttempted;
    int threesMade;
    int threesAttempted;
    int freeThrowsMade;
    int freeThrowsAttempted;
    
    int rebounds;
    int assists;
    int steals;
    int blocks;
    int turnovers;
    int fouls;
    
    // Advanced stats
    float efficientFieldGoal;
    float offensiveRating;
    float defensiveRating;
    float playerEfficiencyRating;
};

} // namespace bm
