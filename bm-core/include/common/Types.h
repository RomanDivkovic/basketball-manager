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

// Match state
struct MatchState {
    std::shared_ptr<Team> homeTeam;
    std::shared_ptr<Team> awayTeam;
    
    int homeScore;
    int awayScore;
    
    int quarterNumber;
    int timeRemaining;  // In seconds
    
    int homeRebounds;
    int awayRebounds;
    
    int homeFouls;
    int awayFouls;
    
    std::shared_ptr<Player> lastScorer;
    std::string lastPlay;
    
    bool matchComplete;
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
