#pragma once

#include "common/Types.h"
#include "engine/TacticalManager.h"
#include <memory>
#include <vector>
#include <string>

namespace bm {
namespace engine {

// Use Team from bm namespace
using bm::Team;
using bm::Player;
using bm::MatchState;

// Speed multiplier enum
enum class SimulationSpeed {
    REAL_TIME = 1,   // x1 - 1 second per game second
    FAST_X2 = 2,     // x2 - 2x faster
    FAST_X3 = 3,     // x3 - 3x faster
    FAST_X4 = 4,     // x4 - 4x faster
    FAST_X6 = 6      // x6 - 6x faster
};

// Interactive menu options when paused
enum class PauseMenuOption {
    RESUME,
    CHANGE_SPEED,
    TIMEOUT,
    SUBSTITUTE,
    SHOW_STATS,
    QUIT
};

/**
 * MatchEngine - Core basketball match simulation (48-minute games)
 * Handles possession, plays, scoring, substitutions, fouls, stamina
 * Supports variable simulation speed (x1 to x6)
 * Integrates tactical management for strategy changes and substitutions
 * Features interactive pause/resume, timeouts, and substitutions
 */
class MatchEngine {
public:
    MatchEngine();
    ~MatchEngine();

    // Initialize match between two teams
    void InitializeMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam);

    // Set simulation speed multiplier (x1, x2, x3, x4, x6)
    void SetSimulationSpeed(SimulationSpeed speed) { speedMultiplier = static_cast<int>(speed); }

    // Get tactical manager for strategy/substitution control
    std::shared_ptr<TacticalManager> GetTacticalManager() { return tacticalManager; }

    // Simulate single possession and return simulated seconds elapsed
    int SimulatePossession();

    // Simulate full quarter (12 minutes = 720 seconds) with interactive pause
    void SimulateQuarter(int quarterNumber);

    // Simulate full match (48 minutes + possible OT)
    void SimulateFullMatch();

    // Simulate match with progress callback (returns remaining time, 0 = complete)
    void SimulateMatchWithSteps(int stepsPerUpdate = 5);

    // Get current match state
    std::shared_ptr<MatchState> GetMatchState() const { return matchState; }

    // Get commentary for current possession
    std::string GetPossessionCommentary() const;

    // Get game progress (0.0 to 1.0)
    float GetMatchProgress() const;

    // Interactive pause menu (call during possession break)
    void HandlePauseMenu();

    // Call timeout for current possession team (30 seconds)
    void CallTimeout(bool isHomeTeam);

    // Get remaining timeouts for a team
    int GetTimeoutsRemaining(bool isHomeTeam) const { 
        return isHomeTeam ? homeTimeoutsRemaining : awayTimeoutsRemaining; 
    }

    // Check if pause is enabled
    bool IsPauseEnabled() const { return pauseEnabled; }
    void SetPauseEnabled(bool enabled) { pauseEnabled = enabled; }

private:
    std::shared_ptr<MatchState> matchState;
    std::shared_ptr<TacticalManager> tacticalManager;
    int speedMultiplier;  // Game speed multiplier (1, 2, 3, 4, 6)
    
    // Pause and timeout state
    bool pauseEnabled;                  // Enable interactive pause
    int homeTimeoutsRemaining;          // Timeouts remaining for home team (3 per half)
    int awayTimeoutsRemaining;          // Timeouts remaining for away team (3 per half)

    // Internal simulation methods
    void PlaySelection();
    void DefenseLogic();
    void ScoringLogic();
    void ApplyFatigue(float secondsPlayed);
    void CheckFouls();
    void HandleSubstitutions();

    // Statistical methods
    float CalculatePlaySuccessRate(const Player& player);
    float CalculateDefenseEffectiveness(const std::vector<std::shared_ptr<Player>>& defenders,
                                        float defensiveIntensity);
    
    // Commentary generation
    std::string GenerateCommentary();

    // Jump ball / tip-off simulation
    void SimulateJumpBall();

    // Format game clock for display
    std::string FormatGameClock(int timeRemaining) const;

    // Ensure we have at least 12 active players by generating placeholders if needed
    void EnsureMinimumRoster(std::shared_ptr<Team> team);
};

} // namespace engine
} // namespace bm
