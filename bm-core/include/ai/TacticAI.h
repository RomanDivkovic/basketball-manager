#pragma once

#include "common/Types.h"
#include <string>
#include <vector>

namespace bm {
namespace ai {

struct Tactic;
using bm::Team;

/**
 * TacticAI - Offensive and defensive play selection
 */
class TacticAI {
public:
    TacticAI();
    ~TacticAI();

    // Select offensive play
    std::string SelectOffensivePlay(const Tactic& currentTactic);

    // Select defensive strategy
    std::string SelectDefensiveStrategy(const Tactic& opponentTactic);

    // Adjust tactics mid-match
    void AdjustTactics(int scoreDiff, int timeRemaining);

private:
    // Play selection strategies
    std::string SelectFromPlaybook(const std::vector<std::string>& plays);
    
    // Statistical models
    float CalculatePlayProbability(const std::string& play);
};

} // namespace ai
} // namespace bm
