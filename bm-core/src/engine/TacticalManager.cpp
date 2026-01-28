#include "engine/TacticalManager.h"
#include <algorithm>
#include <cmath>

namespace bm {
namespace engine {

TacticalManager::TacticalManager()
    : homeOffensiveStrategy(OffensiveStrategy::BALANCED),
      awayOffensiveStrategy(OffensiveStrategy::BALANCED),
      homeDefensiveStrategy(DefensiveStrategy::BALANCED_DEFENSE),
      awayDefensiveStrategy(DefensiveStrategy::BALANCED_DEFENSE),
    homeSubMode(SubstitutionMode::AUTO),
    awaySubMode(SubstitutionMode::AUTO),
    maxMinutesPerPlayer(30),
    fatigueThreshold(0.8f),
    minMinutesBeforeSub(3)
{
}

TacticalManager::~TacticalManager() {
}

void TacticalManager::InitializeForMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam) {
    // Reset to default strategies
    homeOffensiveStrategy = OffensiveStrategy::BALANCED;
    awayOffensiveStrategy = OffensiveStrategy::BALANCED;
    homeDefensiveStrategy = DefensiveStrategy::BALANCED_DEFENSE;
    awayDefensiveStrategy = DefensiveStrategy::BALANCED_DEFENSE;

    // Reset sub logs and plans
    substitutionLogs.clear();
    homeMinutePlan.clear();
    awayMinutePlan.clear();

    // Build default 12-man rotation plans
    BuildMinutePlan(homeTeam, true);
    BuildMinutePlan(awayTeam, false);
}

void TacticalManager::SetHomeOffensiveStrategy(OffensiveStrategy strategy) {
    homeOffensiveStrategy = strategy;
}

void TacticalManager::SetAwayOffensiveStrategy(OffensiveStrategy strategy) {
    awayOffensiveStrategy = strategy;
}

void TacticalManager::SetHomeDefensiveStrategy(DefensiveStrategy strategy) {
    homeDefensiveStrategy = strategy;
}

void TacticalManager::SetAwayDefensiveStrategy(DefensiveStrategy strategy) {
    awayDefensiveStrategy = strategy;
}

bool TacticalManager::ManualSubstitution(std::shared_ptr<Team> team,
                                         std::shared_ptr<Player> playerOut,
                                         std::shared_ptr<Player> playerIn,
                                         int gameTimeSeconds) {
    if (!team || !playerOut || !playerIn) {
        return false;
    }
    
    // Check if playerOut is actually on court
    auto it = std::find(team->roster.begin(), team->roster.end(), playerOut);
    if (it == team->roster.end() || !playerOut->onCourt) {
        return false;
    }
    
    // Check if playerIn is on bench (not on court)
    if (playerIn->onCourt || playerIn->injured) {
        return false;
    }
    
    // Perform substitution
    playerOut->onCourt = false;
    playerIn->onCourt = true;
    
    return true;
}

void TacticalManager::CheckAutoSubstitutions(std::shared_ptr<Team> team, bool isHomeTeam, int gameTimeSeconds) {
    if (!team) {
        return;
    }

    auto mode = isHomeTeam ? homeSubMode : awaySubMode;
    if (mode == SubstitutionMode::MANUAL) {
        return;
    }

    auto& minutePlan = isHomeTeam ? homeMinutePlan : awayMinutePlan;
    
    // Get players on court
    auto playersOnCourt = GetPlayersOnCourt(team);
    
    for (auto& player : playersOnCourt) {
        bool shouldSub = false;
        
        int plannedMinutes = GetPlannedMinutes(player->playerId, isHomeTeam);
        bool overPlanned = player->minutesPlayedThisGame >= (plannedMinutes + 1);
        bool underplayed = player->minutesPlayedThisGame < plannedMinutes * 0.6f;
        
        // Check if player exceeds max minutes
        if (player->minutesPlayedThisGame >= maxMinutesPerPlayer || overPlanned) {
            shouldSub = true;
        }
        
        // Check if player is too tired
        if (player->fatigueLevel >= fatigueThreshold) {
            shouldSub = true;
        }
        
        // If should sub, find replacement
        if (shouldSub && player->minutesPlayedThisGame >= minMinutesBeforeSub) {
            auto replacement = FindBestFreshPlayer(team, player->position, isHomeTeam);
            if (replacement && replacement != player) {
                ManualSubstitution(team, player, replacement, gameTimeSeconds);
                substitutionLogs.push_back("[SUB] " + player->firstName + " " + player->lastName +
                                           " -> " + replacement->firstName + " " + replacement->lastName +
                                           " (fatigue " + std::to_string(static_cast<int>(player->fatigueLevel * 100)) + "%)");
            }
        }

        // Mixed mode: give extra minutes to underplayed players if fresh
        if (!shouldSub && mode == SubstitutionMode::MIXED && underplayed) {
            auto replacement = FindBestFreshPlayer(team, player->position, isHomeTeam);
            if (replacement && replacement != player && replacement->fatigueLevel < player->fatigueLevel - 0.1f) {
                ManualSubstitution(team, player, replacement, gameTimeSeconds);
                substitutionLogs.push_back("[SUB-MIX] " + player->firstName + " " + player->lastName +
                                           " rests, " + replacement->firstName + " " + replacement->lastName + " in");
            }
        }
    }
}

std::vector<std::shared_ptr<Player>> TacticalManager::GetPlayersOnCourt(std::shared_ptr<Team> team) const {
    std::vector<std::shared_ptr<Player>> onCourt;
    for (auto& player : team->roster) {
        if (player->onCourt) {
            onCourt.push_back(player);
        }
    }
    return onCourt;
}

std::vector<std::shared_ptr<Player>> TacticalManager::GetBench(std::shared_ptr<Team> team) const {
    std::vector<std::shared_ptr<Player>> bench;
    for (auto& player : team->roster) {
        if (!player->onCourt && !player->injured && player->active) {
            bench.push_back(player);
        }
    }
    return bench;
}

void TacticalManager::UpdateFatigue(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam) {
    // Fatigue is updated in MatchEngine each possession
    // This method can be used for periodic updates or adjustments
}

float TacticalManager::GetPlayerPerformanceRating(std::shared_ptr<Player> player) const {
    if (!player) return 0.0f;
    
    // Start with base ability
    float rating = player->attributes.currentAbility;
    
    // Apply fatigue penalty (-30% at full fatigue)
    rating *= (1.0f - (player->fatigueLevel * 0.3f));
    
    // Cap between 0-100
    return std::max(0.0f, std::min(100.0f, rating));
}

float TacticalManager::GetThreePointPercentage(OffensiveStrategy strategy) const {
    switch (strategy) {
        case OffensiveStrategy::BALANCED:
            return 0.33f;  // 33% of attempts are 3PT
        case OffensiveStrategy::INSIDE_FOCUSED:
            return 0.15f;  // 15% of attempts are 3PT
        case OffensiveStrategy::THREE_POINT_HEAVY:
            return 0.70f;  // 70% of attempts are 3PT
        case OffensiveStrategy::PACE_AND_SPACE:
            return 0.50f;  // 50% of attempts are 3PT
        case OffensiveStrategy::ISOLATION:
            return 0.40f;  // 40% of attempts are 3PT
        case OffensiveStrategy::FAST_BREAK:
            return 0.45f;  // 45% of attempts are 3PT
        default:
            return 0.33f;
    }
}

float TacticalManager::GetDefensiveIntensity(DefensiveStrategy strategy) const {
    switch (strategy) {
        case DefensiveStrategy::BALANCED_DEFENSE:
            return 1.0f;   // 100% intensity
        case DefensiveStrategy::MAN_TO_MAN:
            return 0.9f;   // 90% intensity
        case DefensiveStrategy::ZONE_2_3:
            return 0.85f;  // 85% intensity
        case DefensiveStrategy::ZONE_3_2:
            return 0.85f;  // 85% intensity
        case DefensiveStrategy::PRESS:
            return 1.3f;   // 130% intensity (exhausting)
        case DefensiveStrategy::PACKED_PAINT:
            return 1.1f;   // 110% intensity
        default:
            return 1.0f;
    }
}

std::shared_ptr<Player> TacticalManager::FindBestFreshPlayer(std::shared_ptr<Team> team, Position pos,
                                                            bool isHomeTeam) {
    std::shared_ptr<Player> bestPlayer = nullptr;
    float bestFreshness = -1.0f;
    auto& minutePlan = isHomeTeam ? homeMinutePlan : awayMinutePlan;
    
    for (auto& player : team->roster) {
        // Check if player is available
        if (player->onCourt || player->injured || !player->active) {
            continue;
        }
        
        // Prefer players at the same position
        if (BenchPlayerEligible(player, pos, isHomeTeam)) {
            float freshness = 1.0f - player->fatigueLevel;
            if (freshness > bestFreshness) {
                bestFreshness = freshness;
                bestPlayer = player;
            }
        }
    }
    
    // If no same-position player available, take any fresh player under plan
    if (!bestPlayer) {
        for (auto& player : team->roster) {
            if (player->onCourt || player->injured || !player->active) {
                continue;
            }
            if (!BenchPlayerEligible(player, pos, isHomeTeam)) {
                continue;
            }
            float freshness = 1.0f - player->fatigueLevel;
            if (freshness > bestFreshness) {
                bestFreshness = freshness;
                bestPlayer = player;
            }
        }
    }
    
    return bestPlayer;
}

int TacticalManager::GetPlayerMinutesOnCourt(std::shared_ptr<Player> player) const {
    if (!player) return 0;
    return static_cast<int>(player->minutesPlayedThisGame);
}

void TacticalManager::BuildMinutePlan(std::shared_ptr<Team> team, bool isHomeTeam) {
    if (!team) return;

    // Sort roster by ability descending
    std::vector<std::shared_ptr<Player>> sorted = team->roster;
    std::sort(sorted.begin(), sorted.end(), [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
        return a->attributes.currentAbility > b->attributes.currentAbility;
    });

    // Use top 12 for rotation
    int rosterSize = static_cast<int>(std::min<size_t>(12, sorted.size()));
    int starters = std::min(5, rosterSize);
    int benchCount = std::max(0, rosterSize - starters);

    // Base assignment: starters 30 minutes
    int remainingMinutes = 240 - (starters * 30);
    int benchMinutes = benchCount > 0 ? std::max(0, remainingMinutes) : 0;
    int baseBench = benchCount > 0 ? benchMinutes / benchCount : 0;
    int bonus = benchCount > 0 ? benchMinutes % benchCount : 0;

    auto& plan = isHomeTeam ? homeMinutePlan : awayMinutePlan;
    plan.clear();

    for (int i = 0; i < rosterSize; ++i) {
        int minutes = 0;
        if (i < starters) {
            minutes = 30;
        } else {
            minutes = baseBench + (bonus > 0 ? 1 : 0);
            if (bonus > 0) bonus--;
        }
        plan[sorted[i]->playerId] = minutes;
    }

    // Deep bench beyond 12 gets 0 planned minutes
    for (size_t i = rosterSize; i < sorted.size(); ++i) {
        plan[sorted[i]->playerId] = 0;
    }
}

int TacticalManager::GetPlannedMinutes(const std::string& playerId, bool isHomeTeam) const {
    const auto& plan = isHomeTeam ? homeMinutePlan : awayMinutePlan;
    auto it = plan.find(playerId);
    if (it == plan.end()) return 24; // Default fallback
    return it->second;
}

bool TacticalManager::BenchPlayerEligible(const std::shared_ptr<Player>& candidate, Position desiredPos,
                                          bool isHomeTeam) const {
    if (!candidate || candidate->onCourt || candidate->injured || !candidate->active) return false;
    auto planned = GetPlannedMinutes(candidate->playerId, isHomeTeam);
    if (planned <= 0) return false;
    if (candidate->minutesPlayedThisGame >= planned + 1) return false;
    // Prefer matching position but allow flexible if none
    return candidate->position == desiredPos || planned >= 15;
}

std::vector<std::string> TacticalManager::ConsumeSubstitutionLogs() {
    auto logs = substitutionLogs;
    substitutionLogs.clear();
    return logs;
}

} // namespace engine
} // namespace bm
