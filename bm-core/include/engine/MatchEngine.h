#pragma once

#include <memory>
#include <vector>
#include <string>

namespace bm {
namespace engine {

// Forward declarations
struct Team;
struct Player;
struct MatchState;

/**
 * MatchEngine - Core basketball match simulation
 * Handles possession, plays, scoring, substitutions, fouls, stamina
 */
class MatchEngine {
public:
    MatchEngine();
    ~MatchEngine();

    // Initialize match between two teams
    void InitializeMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam);

    // Simulate single possession
    void SimulatePossession();

    // Simulate full quarter
    void SimulateQuarter(int quarterNumber);

    // Simulate full match
    void SimulateFullMatch();

    // Get current match state
    std::shared_ptr<MatchState> GetMatchState() const { return matchState; }

    // Get commentary for current possession
    std::string GetPossessionCommentary() const;

private:
    std::shared_ptr<MatchState> matchState;

    // Internal simulation methods
    void PlaySelection();
    void DefenseLogic();
    void ScoringLogic();
    void ApplyFatigue();
    void CheckFouls();
    void HandleSubstitutions();

    // Statistical methods
    float CalculatePlaySuccessRate(const Player& player);
    float CalculateDefenseEffectiveness();
    
    // Commentary generation
    std::string GenerateCommentary();
};

} // namespace engine
} // namespace bm
