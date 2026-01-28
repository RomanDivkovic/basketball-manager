#pragma once

#include "common/Types.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace bm {
namespace engine {

/**
 * Offensive strategies
 */
enum class OffensiveStrategy {
    BALANCED,          // Balanced offense (50% 2PT, 50% 3PT)
    INSIDE_FOCUSED,    // Focus on low-post and 2-point shots
    THREE_POINT_HEAVY, // Heavy 3-point shooting (70% attempts are 3s)
    PACE_AND_SPACE,    // Up-tempo, spread offense
    ISOLATION,         // Isolation-heavy, star player focus
    FAST_BREAK         // Push tempo, transition scoring
};

/**
 * Defensive strategies
 */
enum class DefensiveStrategy {
    BALANCED_DEFENSE,  // Standard man-to-man
    MAN_TO_MAN,        // Strict man-to-man coverage
    ZONE_2_3,          // 2-3 zone
    ZONE_3_2,          // 3-2 zone
    PRESS,             // Full-court press
    PACKED_PAINT       // Defensive focus on paint
};

/**
 * Substitution mode
 */
enum class SubstitutionMode {
    MANUAL,            // Player makes all substitution decisions
    AUTO,              // Automatic based on fatigue and minutes
    MIXED              // Mix of manual and automatic
};

/**
 * TacticalManager - Handles in-game tactical decisions
 * - Manages offensive and defensive strategies
 * - Controls player substitutions (manual and automatic)
 * - Tracks player performance and fatigue
 * - Makes real-time tactical adjustments
 */
class TacticalManager {
public:
    TacticalManager();
    ~TacticalManager();

    // Initialize for a match
    void InitializeForMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam);

    // ===== TACTICAL CHANGES =====
    
    // Set offensive strategy
    void SetHomeOffensiveStrategy(OffensiveStrategy strategy);
    void SetAwayOffensiveStrategy(OffensiveStrategy strategy);
    
    // Set defensive strategy
    void SetHomeDefensiveStrategy(DefensiveStrategy strategy);
    void SetAwayDefensiveStrategy(DefensiveStrategy strategy);
    
    // Get current strategies
    OffensiveStrategy GetHomeOffensiveStrategy() const { return homeOffensiveStrategy; }
    OffensiveStrategy GetAwayOffensiveStrategy() const { return awayOffensiveStrategy; }
    DefensiveStrategy GetHomeDefensiveStrategy() const { return homeDefensiveStrategy; }
    DefensiveStrategy GetAwayDefensiveStrategy() const { return awayDefensiveStrategy; }

    // ===== SUBSTITUTION MANAGEMENT =====
    
    // Set substitution mode
    void SetHomeSubstitutionMode(SubstitutionMode mode) { homeSubMode = mode; }
    void SetAwaySubstitutionMode(SubstitutionMode mode) { awaySubMode = mode; }
    
    // Manual substitution - remove player from court, bring in new player
    bool ManualSubstitution(std::shared_ptr<Team> team, 
                           std::shared_ptr<Player> playerOut,
                           std::shared_ptr<Player> playerIn,
                           int gameTimeSeconds);
    
    // Auto substitution - called each possession to check if subs are needed
    void CheckAutoSubstitutions(std::shared_ptr<Team> team, bool isHomeTeam, int gameTimeSeconds);
    
    // Get current players on court
    std::vector<std::shared_ptr<Player>> GetPlayersOnCourt(std::shared_ptr<Team> team) const;
    
    // Get bench players
    std::vector<std::shared_ptr<Player>> GetBench(std::shared_ptr<Team> team) const;

    // ===== FATIGUE & PERFORMANCE =====
    
    // Update fatigue (called each possession)
    void UpdateFatigue(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam);
    
    // Get fatigue level for a player (0.0 = fresh, 1.0 = exhausted)
    float GetFatigueLevel(std::shared_ptr<Player> player) const { return player->fatigueLevel; }
    
    // Get player performance rating (0-100) based on current form and fatigue
    float GetPlayerPerformanceRating(std::shared_ptr<Player> player) const;

    // ===== AUTO-SUB SETTINGS =====
    
    // Set maximum minutes for a player (will auto-sub when exceeded if AUTO mode)
    void SetMaxMinutesPerPlayer(int minutes) { maxMinutesPerPlayer = minutes; }
    
    // Set fatigue threshold for auto-subs (0-1, default 0.8 = 80% tired)
    void SetFatigueThreshold(float threshold) { fatigueThreshold = threshold; }
    
    // Set minimum minutes on court before allowing auto-sub
    void SetMinMinutesBeforeSub(int minutes) { minMinutesBeforeSub = minutes; }

    // Build minute allocation for a roster (aims for 12-player rotation, 240 total minutes)
    void BuildMinutePlan(std::shared_ptr<Team> team, bool isHomeTeam);

    // Get planned minutes for a player
    int GetPlannedMinutes(const std::string& playerId, bool isHomeTeam) const;

    // Retrieve and clear any substitution commentary queued from auto-subs
    std::vector<std::string> ConsumeSubstitutionLogs();

    // ===== STRATEGY MODIFIERS =====
    
    // Get shooting preference multiplier for strategy
    // Returns % chance of 3-pt attempt vs 2-pt (0.0 to 1.0)
    float GetThreePointPercentage(OffensiveStrategy strategy) const;
    
    // Get defensive intensity multiplier for strategy (affects defense effectiveness)
    float GetDefensiveIntensity(DefensiveStrategy strategy) const;

private:
    // Tactical states
    OffensiveStrategy homeOffensiveStrategy;
    OffensiveStrategy awayOffensiveStrategy;
    DefensiveStrategy homeDefensiveStrategy;
    DefensiveStrategy awayDefensiveStrategy;
    
    // Substitution modes
    SubstitutionMode homeSubMode;
    SubstitutionMode awaySubMode;
    
    // Auto-sub settings
    int maxMinutesPerPlayer;      // Default: 30 minutes
    float fatigueThreshold;        // Default: 0.8 (80% tired)
    int minMinutesBeforeSub;       // Default: 3 minutes

    std::unordered_map<std::string, int> homeMinutePlan;
    std::unordered_map<std::string, int> awayMinutePlan;

    std::vector<std::string> substitutionLogs;
    
    // Helper methods
    std::shared_ptr<Player> FindBestFreshPlayer(std::shared_ptr<Team> team, Position pos,
                                                bool isHomeTeam);
    int GetPlayerMinutesOnCourt(std::shared_ptr<Player> player) const;

    // Determine if bench player is under planned minutes and fresher
    bool BenchPlayerEligible(const std::shared_ptr<Player>& candidate, Position desiredPos,
                             bool isHomeTeam) const;
};

} // namespace engine
} // namespace bm
