#pragma once

#include <memory>
#include <vector>

namespace bm {
namespace ai {

struct Team;
struct Player;

/**
 * AICoordinator - Main AI decision maker
 * Coaching decisions, line-ups, rotations, substitutions
 */
class AICoordinator {
public:
    AICoordinator();
    ~AICoordinator();

    // Initialize AI for team
    void InitializeTeamAI(std::shared_ptr<Team> team);

    // Decide on rotation/line-up
    std::vector<std::shared_ptr<Player>> DecideLineup();

    // Decide substitution
    void DecideSubstitution(int quarterNumber, int currentTime);

    // Timeout decision
    bool ShouldCallTimeout(int scoreDiff);

private:
    std::shared_ptr<Team> managedTeam;

    // Decision algorithms
    float EvaluateLineupChemistry();
    float EvaluatePlayerFatigue();
    float EvaluateMatchupMismatches();
};

} // namespace ai
} // namespace bm
