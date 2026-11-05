#pragma once

#include "common/Types.h"
#include "engine/TacticalManager.h"
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>
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
    COACH_CALLOUT,
    QUIT
};

// Coaching callout options - commands the coach can issue during games
enum class CoachingCallout {
    MORE_OFFENSIVE,    // Ask team to play more aggressively offensively
    MORE_DEFENSIVE,    // Ask team to focus on defense
    MANAGE_TIME,       // Ask team to slow down and manage the clock
    KEEP_GOING         // Ask team to maintain current strategy
};

// Position on the 94x50 ft court
struct CourtPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// How a possession ended
enum class PossessionEnd {
    None,
    MadeShot,
    DefensiveRebound,
    OffensiveRebound,
    Block,
    Steal,
    Turnover,
    ShootingFoul
};

// How the offense came into the ball
enum class PossessionEntry {
    Inbound,   // out of bounds (after a made basket, a foul or a period start)
    Outlet,    // defensive rebound or block recovered by a defender
    LiveBall,  // steal or loose ball
    PutBack    // offensive rebound, same team keeps the ball
};

// Everything decided about one possession before any state is changed
struct PossessionPlan {
    PossessionEnd end = PossessionEnd::None;
    PossessionEntry entry = PossessionEntry::Inbound;
    bool homeOffense = true;
    bool fastBreak = false;
    bool isThree = false;
    bool assisted = false;
    int duration = 0;        // game-clock seconds the possession uses
    int ftAttempts = 0;
    int ftMade = 0;
    std::string actionPhase = "dribble";
    std::string summary;     // one-line description used by the console output
    std::shared_ptr<Team> offense;
    std::shared_ptr<Team> defense;
    std::vector<std::shared_ptr<Player>> onCourtOffense;
    std::vector<std::shared_ptr<Player>> onCourtDefense;
    std::shared_ptr<Player> primary;     // shooter, or the player a pass was meant for
    std::shared_ptr<Player> defender;    // matched defender, stealer, blocker or rebounder
    std::shared_ptr<Player> passer;      // passer of an intercepted pass
    std::shared_ptr<Player> rebounder;   // offensive rebounder
    std::shared_ptr<Player> entryHolder; // player who already has the ball when the possession starts
};

// One choreographed step of a possession; the engine plays it one game second at a time
struct PossessionBeat {
    std::string phase;                   // inbound, advance, pass, set, drive, shot, rebound, steal, turnover, freethrow
    int seconds = 1;
    bool clockRuns = true;
    float speedFactor = 0.75f;           // share of top speed players use
    std::string startText;               // feed line when the beat begins
    std::string endText;                 // feed line after onComplete has changed the score
    std::unordered_map<std::string, CourtPoint> moves;  // player id -> destination
    std::string ballFrom;                // holder until the last second (empty = loose)
    std::string ballTo;                  // holder afterwards; the ball travels in the last second when it differs
    bool ballToHoop = false;             // last second: shot flight towards `hoop`
    CourtPoint hoop;
    std::function<void()> onComplete;    // score and stat changes
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

    // Set simulation speed multiplier (x1, x2, x3, x4, x6, 0 for instant)
    void SetSimulationSpeed(SimulationSpeed speed) { speedMultiplier = static_cast<int>(speed); }
    void SetSpeedMultiplier(int speed) { speedMultiplier = speed; }

    // Get tactical manager for strategy/substitution control
    std::shared_ptr<TacticalManager> GetTacticalManager() { return tacticalManager; }

    // Simulate single possession and return simulated seconds elapsed
    int SimulatePossession();

    // Play exactly one game second of the choreographed match (positions, ball, clock).
    // Returns false once the match is complete.
    bool StepSecond();
    int StepSeconds(int seconds);

    // True while the clock is at 0:00 between periods
    bool IsPeriodBreak() const;

    // Headless mode (used by the UI bridge) disables sleeps and console prompts
    void SetHeadless(bool value) { headless = value; }
    bool IsHeadless() const { return headless; }

    // Play-by-play feed, newest last. feedTotal counts every line ever emitted.
    const std::deque<std::string>& GetFeed() const { return feedHistory; }
    int GetFeedTotal() const { return feedTotal; }

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

    // Apply coaching callout to a team
    void ApplyCoachingCallout(bool isHomeTeam, CoachingCallout callout);

    // Quarter management
    void StartQuarter(int quarterNumber);
    void AdvanceQuarter();

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

    // Coaching momentum tracking
    int homeCoachingMomentum;           // Possessions remaining for home team coaching effect
    int awayCoachingMomentum;           // Possessions remaining for away team coaching effect
    CoachingCallout homeLastCallout;    // Last callout issued to home team
    CoachingCallout awayLastCallout;    // Last callout issued to away team
    static constexpr int COACHING_MOMENTUM_DURATION = 8; // Possessions a callout affects play

    // Live playback state
    bool headless = false;
    std::deque<PossessionBeat> beatQueue;
    PossessionBeat activeBeat;
    bool beatActive = false;
    int activeBeatSecondsLeft = 0;
    int activeBeatSecondIndex = 0;
    PossessionEnd lastEnd = PossessionEnd::None;
    std::string lastBallHolderId;
    std::unordered_map<std::string, std::shared_ptr<Player>> actorById;
    std::deque<std::string> feedHistory;
    int feedTotal = 0;

    // Internal simulation methods
    void PlaySelection();
    void DefenseLogic();
    void ScoringLogic();
    void ApplyFatigue(float secondsPlayed);
    void CheckFouls();
    void HandleSubstitutions();

    // Possession planning and outcome application (shared by the console and live paths)
    bool PlanPossession(PossessionPlan& plan);
    void DetermineEntry(PossessionPlan& plan, PossessionEnd prevEnd, const std::string& prevHolderId);
    void ApplyOutcome(const PossessionPlan& plan);
    void ApplyScore(const PossessionPlan& plan, int points);
    void ApplyFoul(const PossessionPlan& plan);
    void ApplyTurnover(const PossessionPlan& plan, bool steal);
    void ApplyBlock(const PossessionPlan& plan);
    void ApplyRebound(const PossessionPlan& plan, bool offensive);
    void FlipPossession(const PossessionPlan& plan);

    // Live second-by-second playback
    void BuildScript(const PossessionPlan& plan);
    void RebuildCourtRoster();
    void StartNextBeat();
    void PlayBeatSecond();
    void CompleteBeat();
    void FinishPossession();
    void EmitFeed(const std::string& text);

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

    // Update 2D/3D spatial coordinates for players and ball
    void UpdateSpatialState(const std::shared_ptr<Player>& ballHandler,
                            const std::shared_ptr<Player>& primaryDefender,
                            const std::string& actionPhase);
};

} // namespace engine
} // namespace bm
