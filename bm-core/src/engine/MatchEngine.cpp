#include "engine/MatchEngine.h"
#include "engine/TacticalManager.h"
#include "common/Types.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <thread>

namespace bm {
namespace engine {

// Random number generator
static std::random_device rd;
static std::mt19937 gen(rd());

// Helper function to get random float [0, 1)
static float RandomFloat() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

// Helper to get random float between min and max
static float RandomFloatRange(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

// Helper function to get random int [min, max]
static int RandomInt(int min, int max) {
    if (min > max) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

MatchEngine::MatchEngine() : matchState(nullptr), speedMultiplier(1), pauseEnabled(false), 
                             homeTimeoutsRemaining(7), awayTimeoutsRemaining(7),
                             homeCoachingMomentum(0), awayCoachingMomentum(0),
                             homeLastCallout(CoachingCallout::KEEP_GOING), 
                             awayLastCallout(CoachingCallout::KEEP_GOING) {
    std::cout << "[MatchEngine] Initialized\n";
    tacticalManager = std::make_shared<TacticalManager>();
}

MatchEngine::~MatchEngine() {
}

void MatchEngine::InitializeMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam) {
    std::cout << "[MatchEngine] Initializing 48-minute match: " << homeTeam->name << " vs " << awayTeam->name << "\n";

    // Guarantee a 12-man rotation for each side (pad with placeholders if needed)
    EnsureMinimumRoster(homeTeam);
    EnsureMinimumRoster(awayTeam);
    
    // Initialize tactical manager
    tacticalManager->InitializeForMatch(homeTeam, awayTeam);
    
    // Create new match state
    matchState = std::make_shared<MatchState>();
    matchState->homeTeam = homeTeam;
    matchState->awayTeam = awayTeam;
    matchState->homeScore = 0;
    matchState->awayScore = 0;
    matchState->quarterNumber = 1;
    matchState->timeRemaining = 720;  // 12 minutes per quarter (720 seconds)
    matchState->timeElapsed = 0;
    matchState->homeRebounds = 0;
    matchState->awayRebounds = 0;
    matchState->homeFouls = 0;
    matchState->awayFouls = 0;
    matchState->homeSteals = 0;
    matchState->awaySteals = 0;
    matchState->homeBlocks = 0;
    matchState->awayBlocks = 0;
    matchState->homeTurnovers = 0;
    matchState->awayTurnovers = 0;
    matchState->homeTeamPossession = true;  // Home team starts
    matchState->jumpBallWinner = true;      // Will be set during jump ball
    matchState->possessionCount = 0;
    matchState->matchComplete = false;
    
    // NBA: 7 timeouts per team for regulation
    homeTimeoutsRemaining = 7;
    awayTimeoutsRemaining = 7;
    pendingTimeoutHome = pendingTimeoutAway = 0;
    homeQ4Timeouts = awayQ4Timeouts = homeLateTimeouts = awayLateTimeouts = 0;
    homeRun = awayRun = 0;
    lastAiTimeoutElapsed = -1000;
    currentBreak = BreakType::None;
    breakTalkGiven = false;
    breakConsumed = false;
    lastTalk = TeamTalkResult{};
    offenseTarget[0].clear();
    offenseTarget[1].clear();
    offensePlay[0].clear();
    offensePlay[1].clear();
    instructions.clear();
    lastFeedback = PlayerFeedbackResult{};
    pendingSubs.clear();
    beatQueue.clear();
    beatActive = false;
    lastEnd = PossessionEnd::None;
    lastBallHolderId.clear();
    feedHistory.clear();
    feedTotal = 0;
    
    // Initialize starting five (best 5 available by ability)
    auto setupStartingFive = [](std::shared_ptr<Team> team, std::vector<std::shared_ptr<Player>>& lineup) {
        std::vector<std::shared_ptr<Player>> sorted;
        for (auto& p : team->roster) {
            if (!p->injured && p->active) {
                sorted.push_back(p);
            }
            // Reset per-game stats
            p->onCourt = false;
            p->minutesPlayedThisGame = 0;
            p->fatigueLevel = 0.0f;
            p->pointsThisGame = 0;
            p->reboundsThisGame = 0;
            p->assistsThisGame = 0;
            p->stealsThisGame = 0;
            p->blocksThisGame = 0;
            p->turnoversThisGame = 0;
            p->foulsThisGame = 0;
            p->morale = p->seasonMood * 0.5f;
        }

        std::sort(sorted.begin(), sorted.end(), [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
            return a->attributes.currentAbility > b->attributes.currentAbility;
        });

        int count = 0;
        for (auto& player : sorted) {
            if (count >= 5) break;
            player->onCourt = true;
            lineup.push_back(player);
            count++;
        }
    };

    setupStartingFive(homeTeam, matchState->homeStartingFive);
    setupStartingFive(awayTeam, matchState->awayStartingFive);

    // Initialize initial spatial court positions (tip-off formation)
    UpdateSpatialState(nullptr, nullptr, "tipoff");
    
    std::cout << "[MatchEngine] Match initialized - " << homeTeam->name << " 0-0 " << awayTeam->name << "\n";
}

std::string MatchEngine::FormatGameClock(int timeRemaining) const {
    int minutes = timeRemaining / 60;
    int seconds = timeRemaining % 60;
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
    return oss.str();
}

void MatchEngine::SimulateJumpBall() {
    if (!matchState) return;

    // Find centers (or tallest players) from each team
    std::shared_ptr<Player> homeCenter = nullptr;
    std::shared_ptr<Player> awayCenter = nullptr;

    // Get home center (prefer actual C, fallback to tallest/best physical player)
    for (auto& p : matchState->homeTeam->roster) {
        if (p->onCourt) {
            if (p->position == Position::C) {
                homeCenter = p;
                break;
            }
            if (!homeCenter || p->attributes.physical > homeCenter->attributes.physical) {
                homeCenter = p;
            }
        }
    }

    // Get away center
    for (auto& p : matchState->awayTeam->roster) {
        if (p->onCourt) {
            if (p->position == Position::C) {
                awayCenter = p;
                break;
            }
            if (!awayCenter || p->attributes.physical > awayCenter->attributes.physical) {
                awayCenter = p;
            }
        }
    }

    if (!homeCenter || !awayCenter) return;

    // Jump ball contest based on physical + ballControl
    float homeJumpRating = homeCenter->attributes.physical + homeCenter->attributes.ballControl * 0.5f;
    float awayJumpRating = awayCenter->attributes.physical + awayCenter->attributes.ballControl * 0.5f;

    // Add some randomness (±20%)
    homeJumpRating *= (0.9f + RandomFloat() * 0.2f);
    awayJumpRating *= (0.9f + RandomFloat() * 0.2f);

    bool homeWins = homeJumpRating > awayJumpRating;
    matchState->homeTeamPossession = homeWins;
    matchState->jumpBallWinner = homeWins;  // Track Q1 tip-off winner for possession arrow

    // Find a point guard to tip to (or any guard)
    auto winningTeam = homeWins ? matchState->homeTeam : matchState->awayTeam;
    std::shared_ptr<Player> pointGuard = nullptr;

    for (auto& p : winningTeam->roster) {
        if (p->onCourt && (p->position == Position::PG || p->position == Position::SG)) {
            pointGuard = p;
            if (p->position == Position::PG) break; // Prefer PG
        }
    }

    std::string winnerName = homeWins ? matchState->homeTeam->name : matchState->awayTeam->name;
    std::string centerName = homeWins ? homeCenter->lastName : awayCenter->lastName;
    std::string guardName = pointGuard ? pointGuard->lastName : centerName;

    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                         🏀 TIP-OFF 🏀                         ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::left << std::setw(60) << (centerName + " wins the tip, taps to " + guardName + "!") << " ║\n";
    std::cout << "║  " << std::left << std::setw(60) << (winnerName + " will start with possession") << " ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";

    if (!headless && speedMultiplier > 0 && speedMultiplier < 6) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800 / speedMultiplier));
    }
}

// Players ordered by role (PG..C), best ball handler first inside a role; used to pair matchups and floor spots
static std::vector<std::shared_ptr<Player>> SortByRole(std::vector<std::shared_ptr<Player>> players) {
    std::stable_sort(players.begin(), players.end(), [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
        if (a->position != b->position) return static_cast<int>(a->position) < static_cast<int>(b->position);
        return a->attributes.ballControl > b->attributes.ballControl;
    });
    return players;
}

// Weighted random pick; falls back to the first candidate when every weight is zero
template <typename WeightFn>
static std::shared_ptr<Player> PickWeighted(const std::vector<std::shared_ptr<Player>>& candidates, WeightFn weightOf) {
    if (candidates.empty()) return nullptr;
    float total = 0.0f;
    for (const auto& p : candidates) total += std::max(0.0f, weightOf(p));
    if (total <= 0.0f) return candidates.front();
    float target = RandomFloat() * total;
    for (const auto& p : candidates) {
        target -= std::max(0.0f, weightOf(p));
        if (target <= 0.0f) return p;
    }
    return candidates.back();
}

void MatchEngine::EmitFeed(const std::string& text) {
    if (!matchState || text.empty()) return;
    std::ostringstream line;
    line << "[Q" << matchState->quarterNumber << " " << FormatGameClock(matchState->timeRemaining) << "] ("
         << matchState->homeScore << "-" << matchState->awayScore << ") " << text;
    feedHistory.push_back(line.str());
    ++feedTotal;
    if (feedHistory.size() > 200) feedHistory.pop_front();
    matchState->lastPlay = text;
}

void MatchEngine::DetermineEntry(PossessionPlan& plan, PossessionEnd prevEnd, const std::string& prevHolderId) {
    plan.entry = PossessionEntry::Inbound;
    plan.entryHolder = nullptr;

    std::shared_ptr<Player> holder;
    if (!prevHolderId.empty()) {
        for (const auto& p : plan.onCourtOffense) {
            if (p->playerId == prevHolderId) holder = p;
        }
    }

    float fastBreakChance = 0.0f;
    switch (prevEnd) {
        case PossessionEnd::Steal:
        case PossessionEnd::Turnover:
            if (holder) {
                plan.entry = PossessionEntry::LiveBall;
                plan.entryHolder = holder;
                fastBreakChance = (prevEnd == PossessionEnd::Steal) ? 0.60f : 0.30f;
            }
            break;
        case PossessionEnd::DefensiveRebound:
        case PossessionEnd::Block:
            if (holder) {
                plan.entry = PossessionEntry::Outlet;
                plan.entryHolder = holder;
                fastBreakChance = 0.20f;
            }
            break;
        case PossessionEnd::OffensiveRebound:
            if (holder) {
                plan.entry = PossessionEntry::PutBack;
                plan.entryHolder = holder;
            }
            break;
        case PossessionEnd::MadeShot:
            fastBreakChance = 0.04f;
            break;
        default:
            break;
    }

    if (plan.entry == PossessionEntry::PutBack || fastBreakChance <= 0.0f) return;

    auto offenseStrategy = plan.homeOffense ? tacticalManager->GetHomeOffensiveStrategy()
                                            : tacticalManager->GetAwayOffensiveStrategy();
    if (offenseStrategy == OffensiveStrategy::FAST_BREAK) fastBreakChance += 0.25f;
    else if (offenseStrategy == OffensiveStrategy::PACE_AND_SPACE) fastBreakChance += 0.10f;

    plan.fastBreak = RandomFloat() < fastBreakChance;
}

bool MatchEngine::PlanPossession(PossessionPlan& plan) {
    if (!matchState || matchState->matchComplete) return false;

    PossessionEnd prevEnd = lastEnd;
    const std::string prevHolderId = lastBallHolderId;

    // Check if current quarter is out of time
    if (matchState->timeRemaining <= 0) {
        if (matchState->quarterNumber < 4) {
            if (!breaksEnabled) EmitFeed("End of quarter " + std::to_string(matchState->quarterNumber) + ".");
            AdvanceQuarter();
            breakConsumed = false;
            prevEnd = PossessionEnd::None;
        } else if (matchState->homeScore != matchState->awayScore) {
            matchState->matchComplete = true;
            EmitFeed("Final buzzer!");
            return false;
        } else {
            if (!breaksEnabled) EmitFeed("Tied after regulation, overtime!");
            AdvanceQuarter();
            breakConsumed = false;
            prevEnd = PossessionEnd::None;
        }
    }

    // Dead ball: queued substitutions come on, instructions wear off
    ApplyPendingSubstitutions();
    for (auto it = instructions.begin(); it != instructions.end();) {
        if (--it->second.possessionsLeft <= 0) it = instructions.erase(it);
        else ++it;
    }

    plan = PossessionPlan{};
    matchState->possessionCount++;

    plan.homeOffense = matchState->homeTeamPossession;
    plan.offense = plan.homeOffense ? matchState->homeTeam : matchState->awayTeam;
    plan.defense = plan.homeOffense ? matchState->awayTeam : matchState->homeTeam;
    for (auto& p : plan.offense->roster) {
        if (p->onCourt) plan.onCourtOffense.push_back(p);
    }
    for (auto& p : plan.defense->roster) {
        if (p->onCourt) plan.onCourtDefense.push_back(p);
    }

    DetermineEntry(plan, prevEnd, prevHolderId);

    // Variable possession duration to drive pace (~12-20 seconds); fast breaks and put-backs are quicker
    int durationSeconds;
    if (plan.fastBreak) {
        durationSeconds = RandomInt(5, 9);
    } else if (plan.entry == PossessionEntry::PutBack) {
        durationSeconds = RandomInt(4, 9);
    } else {
        durationSeconds = std::max(6, static_cast<int>(std::round(RandomFloatRange(12.0f, 20.0f))));
    }
    plan.duration = std::min(durationSeconds, std::max(1, matchState->timeRemaining));

    if (plan.onCourtOffense.empty() || plan.onCourtDefense.empty()) {
        plan.end = PossessionEnd::None;
        lastEnd = PossessionEnd::None;
        lastBallHolderId.clear();
        return true;
    }

    auto& onCourtOffense = plan.onCourtOffense;
    auto& onCourtDefense = plan.onCourtDefense;

    // Usage: better shooters and ball handlers take more of the possessions
    auto usageWeight = [&](const std::shared_ptr<Player>& p) {
        float w = 1.0f + p->attributes.shooting * 0.9f + p->attributes.ballControl * 0.45f
                  + p->attributes.technical * 0.2f + p->attributes.currentAbility * 0.03f;
        w *= (1.0f - p->fatigueLevel * 0.4f);
        const bool liveEntry = plan.fastBreak || plan.entry == PossessionEntry::PutBack;
        if (liveEntry && plan.entryHolder && p == plan.entryHolder) w *= 2.5f;

        // Play through a player: the hub takes a much larger share
        const auto scheme = plan.homeOffense ? tacticalManager->GetHomeOffensiveStrategy()
                                             : tacticalManager->GetAwayOffensiveStrategy();
        if (scheme == OffensiveStrategy::PLAY_THROUGH_PLAYER && p->playerId == offenseTarget[plan.homeOffense ? 0 : 1]) {
            w *= 2.4f;
        }
        // Individual feedback
        w *= 1.0f + 0.5f * InstructionStrength(p->playerId, PlayerInstruction::LookForYourShot)
                  - 0.3f * InstructionStrength(p->playerId, PlayerInstruction::FocusOnDefense);
        return std::max(0.1f, w);
    };
    auto ballHandler = PickWeighted(onCourtOffense, usageWeight);

    // Mostly the matched defender, sometimes a help defender
    std::shared_ptr<Player> defender;
    if (RandomFloat() < 0.65f) {
        auto offenseByRole = SortByRole(onCourtOffense);
        auto defenseByRole = SortByRole(onCourtDefense);
        for (size_t k = 0; k < offenseByRole.size() && k < defenseByRole.size(); ++k) {
            if (offenseByRole[k] == ballHandler) defender = defenseByRole[k];
        }
    }
    if (!defender) {
        defender = onCourtDefense[RandomInt(0, static_cast<int>(onCourtDefense.size()) - 1)];
    }

    // Passer: a playmaker other than the ball handler
    std::vector<std::shared_ptr<Player>> teammates;
    for (auto& p : onCourtOffense) {
        if (p != ballHandler) teammates.push_back(p);
    }
    auto passer = teammates.empty()
        ? ballHandler
        : PickWeighted(teammates, [](const std::shared_ptr<Player>& p) {
              return 1.0f + p->attributes.technical * 0.6f + p->attributes.ballControl * 0.6f;
          });

    plan.primary = ballHandler;
    plan.defender = defender;
    plan.passer = passer;

    // Tactical context
    auto offenseStrategy = plan.homeOffense ? tacticalManager->GetHomeOffensiveStrategy()
                                            : tacticalManager->GetAwayOffensiveStrategy();
    auto defenseStrategy = plan.homeOffense ? tacticalManager->GetAwayDefensiveStrategy()
                                            : tacticalManager->GetHomeDefensiveStrategy();

    float defensiveIntensity = tacticalManager->GetDefensiveIntensity(defenseStrategy);
    float offensiveRating = CalculatePlaySuccessRate(*ballHandler);
    float defensiveRating = CalculateDefenseEffectiveness(onCourtDefense, defensiveIntensity);

    // Fatigue impacts
    offensiveRating *= (1.0f - (ballHandler->fatigueLevel * 0.25f));
    defensiveRating *= (1.0f - (defender->fatigueLevel * 0.15f));

    // Mood: a fired-up shooter converts more, a team that tuned the coach out defends worse
    float defenseMorale = 0.0f;
    for (const auto& d : onCourtDefense) defenseMorale += d->morale;
    defenseMorale /= static_cast<float>(onCourtDefense.size());
    offensiveRating *= (1.0f + 0.10f * ballHandler->morale);
    defensiveRating *= (1.0f + 0.08f * defenseMorale);
    for (auto& p : onCourtOffense) p->morale *= 0.985f;
    for (auto& p : onCourtDefense) p->morale *= 0.985f;

    // Defenders told to focus on defense
    for (const auto& d : onCourtDefense) {
        defensiveRating *= 1.0f + 0.04f * InstructionStrength(d->playerId, PlayerInstruction::FocusOnDefense);
    }
    float schemeEfficiency = 0.0f;   // shot quality from the scheme, the called play and feedback
    float assistBonus = 0.0f;

    // Base chances
    float stealChance = 0.08f + std::max(0.0f, defensiveIntensity - 1.0f) * 0.04f;
    float foulChance = 0.12f + std::max(0.0f, defensiveIntensity - 1.0f) * 0.03f;
    float turnoverChance = 0.06f;

    // Quick possessions leave little time for a pass to be picked off
    if (plan.fastBreak) {
        stealChance *= 0.5f;
        turnoverChance *= 0.6f;
    } else if (plan.entry == PossessionEntry::PutBack) {
        stealChance *= 0.4f;
        turnoverChance *= 0.5f;
    }

    // Shot selection probabilities
    float threePointChance = tacticalManager->GetThreePointPercentage(offenseStrategy);
    if (ballHandler->position == Position::PG || ballHandler->position == Position::SG) {
        threePointChance += 0.05f;
    }
    if (plan.fastBreak) threePointChance *= 0.5f;

    // Called play (or the plain scheme) and individual feedback
    const Play* activePlay = nullptr;
    const std::string& calledPlay = offensePlay[plan.homeOffense ? 0 : 1];
    if (!calledPlay.empty()) {
        for (const auto& candidate : GetPlaybook()) {
            if (candidate.id == calledPlay && candidate.scheme == offenseStrategy) activePlay = &candidate;
        }
    }
    if (activePlay) {
        threePointChance += activePlay->threeBias;
        schemeEfficiency += activePlay->efficiency;
        turnoverChance += activePlay->turnover;
    } else if (offenseStrategy == OffensiveStrategy::PICK_AND_ROLL) {
        schemeEfficiency += 0.025f;
    } else if (offenseStrategy == OffensiveStrategy::BALL_MOVEMENT) {
        schemeEfficiency += 0.02f;
        turnoverChance += 0.01f;
    }
    if (offenseStrategy == OffensiveStrategy::BALL_MOVEMENT) assistBonus = 0.12f;
    else if (offenseStrategy == OffensiveStrategy::PICK_AND_ROLL) assistBonus = 0.05f;
    turnoverChance -= 0.03f * InstructionStrength(ballHandler->playerId, PlayerInstruction::SettleDown);
    schemeEfficiency -= 0.015f * InstructionStrength(ballHandler->playerId, PlayerInstruction::LookForYourShot);
    turnoverChance = std::max(0.01f, turnoverChance);
    threePointChance = std::min(0.8f, std::max(0.1f, threePointChance));

    // Build log
    std::ostringstream oss;

    float roll = RandomFloat();

    if (roll < stealChance) {
        // Steal/turnover
        plan.end = PossessionEnd::Steal;
        plan.actionPhase = "steal";
        oss << passer->firstName << " swings to " << ballHandler->lastName << ", stolen by "
            << defender->lastName << "!";
    } else if (roll < stealChance + foulChance) {
        // Shooting foul leads to free throws
        plan.end = PossessionEnd::ShootingFoul;
        plan.actionPhase = "foul";
        float ftPct = std::clamp(0.72f + (ballHandler->attributes.shooting / 20.0f) * 0.1f, 0.6f, 0.9f);
        int attempts = 2;
        // Occasional and-1 or three-shot foul
        if (RandomFloat() < 0.1f) attempts = 3;
        int made = 0;
        for (int i = 0; i < attempts; ++i) {
            if (RandomFloat() < ftPct) made++;
        }
        plan.ftAttempts = attempts;
        plan.ftMade = made;
        oss << ballHandler->firstName << " " << ballHandler->lastName << " fouled by "
            << defender->lastName << " - hits " << made << "/" << attempts << " at the line.";
    } else if (roll < stealChance + foulChance + turnoverChance) {
        // Unforced turnover
        plan.end = PossessionEnd::Turnover;
        plan.actionPhase = "turnover";
        oss << ballHandler->firstName << " " << ballHandler->lastName << " loses the handle (turnover).";
    } else {
        // Shot attempt
        plan.actionPhase = "shot";
        plan.isThree = (RandomFloat() < threePointChance);
        float base2pt = 0.44f + (ballHandler->attributes.shooting / 20.0f) * 0.14f;
        base2pt *= (1.0f + (ballHandler->attributes.technical / 20.0f) * 0.05f);
        float fg2 = std::clamp(base2pt * (offensiveRating / 0.5f) * (1.0f - defensiveRating * 0.15f)
                               * (1.0f - ballHandler->fatigueLevel * 0.2f), 0.35f, 0.7f);
        // Open-floor finishes are easier
        if (plan.fastBreak && !plan.isThree) fg2 = std::min(0.75f, fg2 * 1.10f);
        fg2 = std::clamp(fg2 + schemeEfficiency, 0.30f, 0.78f);
        float fg3 = std::clamp(fg2 - 0.08f, 0.26f, 0.55f);
        float shotMakeProb = plan.isThree ? fg3 : fg2;

        float blockChance = std::clamp(0.04f + defensiveIntensity * 0.03f, 0.02f, 0.12f);

        if (RandomFloat() < blockChance) {
            // Blocked shot -> defensive rebound
            plan.end = PossessionEnd::Block;
            plan.actionPhase = "block";
            oss << defender->lastName << " rejects " << ballHandler->lastName << "!";
        } else if (RandomFloat() < shotMakeProb) {
            plan.end = PossessionEnd::MadeShot;
            plan.actionPhase = plan.isThree ? "three_make" : "shot_make";
            plan.assisted = onCourtOffense.size() > 1 && RandomFloat() < ((plan.isThree ? 0.82f : 0.58f) + assistBonus);
            if (plan.assisted) {
                oss << passer->firstName << " finds " << ballHandler->lastName << " for "
                    << (plan.isThree ? "a 3!" : "two!");
            } else {
                oss << ballHandler->lastName << " scores " << (plan.isThree ? "a 3!" : "two!");
            }
        } else {
            // Missed shot
            plan.actionPhase = "miss";
            float offensiveRebChance = std::clamp(0.27f - (defensiveIntensity - 1.0f) * 0.06f, 0.18f, 0.32f);
            bool offensiveRebound = (RandomFloat() < offensiveRebChance);

            if (offensiveRebound) {
                plan.end = PossessionEnd::OffensiveRebound;
                plan.rebounder = PickWeighted(onCourtOffense, [](const std::shared_ptr<Player>& p) {
                    return 1.0f + p->attributes.physical;
                });
                oss << ballHandler->lastName << " misses, " << plan.rebounder->lastName << " with the offensive board.";
            } else {
                plan.end = PossessionEnd::DefensiveRebound;
                // Bigger, stronger defenders win more of the boards
                plan.defender = PickWeighted(onCourtDefense, [](const std::shared_ptr<Player>& p) {
                    return 1.0f + p->attributes.physical;
                });
                oss << ballHandler->lastName << " misses, rebound " << plan.defender->lastName << ".";
            }
        }
    }

    plan.summary = oss.str();

    // Remember how this possession ended so the next one can start from the right place
    lastEnd = plan.end;
    switch (plan.end) {
        case PossessionEnd::Steal:
        case PossessionEnd::Turnover:
        case PossessionEnd::Block:
        case PossessionEnd::DefensiveRebound:
            lastBallHolderId = plan.defender ? plan.defender->playerId : std::string();
            break;
        case PossessionEnd::OffensiveRebound:
            lastBallHolderId = plan.rebounder ? plan.rebounder->playerId : std::string();
            break;
        default:
            lastBallHolderId.clear();
            break;
    }
    return true;
}

void MatchEngine::FlipPossession(const PossessionPlan& plan) {
    matchState->homeTeamPossession = !plan.homeOffense;
}

void MatchEngine::ApplyScore(const PossessionPlan& plan, int points) {
    if (plan.homeOffense) {
        matchState->homeScore += points;
        if (points > 0) { homeRun += points; awayRun = 0; }
    } else {
        matchState->awayScore += points;
        if (points > 0) { awayRun += points; homeRun = 0; }
    }
    plan.primary->pointsThisGame += points;
    matchState->lastScorer = plan.primary;
}

void MatchEngine::ApplyFoul(const PossessionPlan& plan) {
    if (plan.homeOffense) {
        matchState->awayFouls++;
    } else {
        matchState->homeFouls++;
    }
    plan.defender->foulsThisGame++;
}

void MatchEngine::ApplyTurnover(const PossessionPlan& plan, bool steal) {
    if (steal) plan.defender->stealsThisGame++;
    plan.primary->turnoversThisGame++;
    if (plan.homeOffense) {
        matchState->homeTurnovers++;
        if (steal) matchState->awaySteals++;
    } else {
        matchState->awayTurnovers++;
        if (steal) matchState->homeSteals++;
    }
    FlipPossession(plan);
}

void MatchEngine::ApplyBlock(const PossessionPlan& plan) {
    plan.defender->blocksThisGame++;
    plan.defender->reboundsThisGame++;
    matchState->lastRebounder = plan.defender;
    if (plan.homeOffense) {
        matchState->awayBlocks++;
        matchState->awayRebounds++;
    } else {
        matchState->homeBlocks++;
        matchState->homeRebounds++;
    }
    FlipPossession(plan);
}

void MatchEngine::ApplyRebound(const PossessionPlan& plan, bool offensive) {
    if (offensive) {
        plan.rebounder->reboundsThisGame++;
        matchState->lastRebounder = plan.rebounder;
        if (plan.homeOffense) {
            matchState->homeRebounds++;
        } else {
            matchState->awayRebounds++;
        }
        return;
    }

    plan.defender->reboundsThisGame++;
    matchState->lastRebounder = plan.defender;
    if (plan.homeOffense) {
        matchState->awayRebounds++;
    } else {
        matchState->homeRebounds++;
    }
    FlipPossession(plan);
}

// Applies the whole possession at once (console and background simulation)
void MatchEngine::ApplyOutcome(const PossessionPlan& plan) {
    switch (plan.end) {
        case PossessionEnd::Steal:
            ApplyTurnover(plan, true);
            break;
        case PossessionEnd::Turnover:
            ApplyTurnover(plan, false);
            break;
        case PossessionEnd::ShootingFoul:
            ApplyFoul(plan);
            ApplyScore(plan, plan.ftMade);
            FlipPossession(plan);
            break;
        case PossessionEnd::Block:
            ApplyBlock(plan);
            break;
        case PossessionEnd::MadeShot:
            ApplyScore(plan, plan.isThree ? 3 : 2);
            if (plan.assisted && plan.passer) plan.passer->assistsThisGame++;
            FlipPossession(plan);
            break;
        case PossessionEnd::OffensiveRebound:
            ApplyRebound(plan, true);
            break;
        case PossessionEnd::DefensiveRebound:
            ApplyRebound(plan, false);
            break;
        case PossessionEnd::None:
            break;
    }
}

int MatchEngine::SimulatePossession() {
    PossessionPlan plan;
    if (!PlanPossession(plan)) return 0;

    if (plan.end == PossessionEnd::None) {
        // No lineup on the floor: still run the clock so the match can finish
        matchState->timeRemaining = std::max(0, matchState->timeRemaining - plan.duration);
        matchState->timeElapsed += plan.duration;
        return plan.duration;
    }

    ApplyOutcome(plan);
    const int durationSeconds = plan.duration;
    matchState->lastPlay = plan.summary;

    // Update spatial player/ball coordinates for 2D/3D visualization
    UpdateSpatialState(plan.primary, plan.defender, plan.actionPhase);

    // Deduct possession time
    matchState->timeRemaining -= durationSeconds;
    matchState->timeElapsed += durationSeconds;
    if (matchState->timeRemaining < 0) matchState->timeRemaining = 0;

    // Apply fatigue to all players on court
    ApplyFatigue(static_cast<float>(durationSeconds));

    // Check auto subs for both teams with planned minutes
    tacticalManager->CheckAutoSubstitutions(matchState->homeTeam, true, matchState->timeElapsed);
    tacticalManager->CheckAutoSubstitutions(matchState->awayTeam, false, matchState->timeElapsed);

    // Surface substitution logs in console
    auto subLogs = tacticalManager->ConsumeSubstitutionLogs();
    for (const auto& log : subLogs) {
        std::cout << log << "\n";
    }

    // Output possession play-by-play with game clock and score
    std::string clock = FormatGameClock(matchState->timeRemaining);
    std::cout << "[Q" << matchState->quarterNumber << " " << clock << "] "
              << std::setw(3) << matchState->homeScore << "-"
              << std::setw(3) << std::left << matchState->awayScore << " | "
              << matchState->lastPlay << "\n";

    // Optional pacing delay based on speed multiplier (skip delay if speedMultiplier <= 0 or for background sim >= 6)
    int baseDelayMs = 400;
    if (!headless && speedMultiplier > 0 && speedMultiplier < 6) {
        std::this_thread::sleep_for(std::chrono::milliseconds(baseDelayMs / speedMultiplier));
    }

    return durationSeconds;
}

void MatchEngine::ApplyFatigue(float secondsPlayed) {
    if (!matchState) return;

    // Increase fatigue for players on court
    const float FATIGUE_RATE_PER_SECOND = 1.0f / 2880.0f; // full fatigue over 48 minutes

    for (auto& player : matchState->homeTeam->roster) {
        if (player->onCourt) {
            player->fatigueLevel += FATIGUE_RATE_PER_SECOND * secondsPlayed;
            player->minutesPlayedThisGame += secondsPlayed / 60.0f;
            player->fatigueLevel = std::min(1.0f, player->fatigueLevel);
        }
    }

    for (auto& player : matchState->awayTeam->roster) {
        if (player->onCourt) {
            player->fatigueLevel += FATIGUE_RATE_PER_SECOND * secondsPlayed;
            player->minutesPlayedThisGame += secondsPlayed / 60.0f;
            player->fatigueLevel = std::min(1.0f, player->fatigueLevel);
        }
    }
}

void MatchEngine::StartQuarter(int quarterNumber) {
    if (!matchState) return;

    matchState->quarterNumber = quarterNumber;
    matchState->timeRemaining = (quarterNumber <= 4) ? 720 : 300; // 12 min regulation, 5 min OT

    // Overtime: two timeouts per team (NBA rule 5, section VI)
    if (quarterNumber >= 5) {
        homeTimeoutsRemaining = 2;
        awayTimeoutsRemaining = 2;
    }

    std::cout << "\n═════════════════ QUARTER " << quarterNumber << " ═════════════════\n";

    // Possession arrow rules
    if (quarterNumber == 1) {
        SimulateJumpBall();
    } else if (quarterNumber >= 5) {
        SimulateJumpBall();
    } else {
        if (quarterNumber == 2 || quarterNumber == 3) {
            matchState->homeTeamPossession = !matchState->jumpBallWinner;
            std::string startingTeam = matchState->homeTeamPossession ? 
                matchState->homeTeam->name : matchState->awayTeam->name;
            std::cout << "\n[POSSESSION ARROW] " << startingTeam << " starts with the ball\n";
        } else if (quarterNumber == 4) {
            matchState->homeTeamPossession = matchState->jumpBallWinner;
            std::string startingTeam = matchState->homeTeamPossession ? 
                matchState->homeTeam->name : matchState->awayTeam->name;
            std::cout << "\n[POSSESSION ARROW] " << startingTeam << " starts with the ball\n";
        }
    }
}

void MatchEngine::AdvanceQuarter() {
    if (!matchState) return;
    int nextQuarter = matchState->quarterNumber + 1;
    StartQuarter(nextQuarter);
}

void MatchEngine::SimulateQuarter(int quarterNumber) {
    if (!matchState) return;
    
    StartQuarter(quarterNumber);

    // Run until the quarter clock expires
    while (matchState->timeRemaining > 0) {
        SimulatePossession();
        
        // Check for user pause (only if enabled)
        if (pauseEnabled && matchState->possessionCount % 3 == 0) {
            // Every 3 possessions, check if user wants to pause
            std::cout << "\n[Press 'p' + ENTER to pause, or just ENTER to continue]: ";
            std::string input;
            std::getline(std::cin, input);
            if (input == "p" || input == "P") {
                HandlePauseMenu();
            }
        }
    }
    
    // Print quarter summary
    std::cout << "Q" << quarterNumber << " FINAL: " << matchState->homeTeam->name << " " 
              << matchState->homeScore << " - " << matchState->awayScore << " " 
              << matchState->awayTeam->name << "\n";
}

void MatchEngine::SimulateFullMatch() {
    if (!matchState) {
        std::cout << "[MatchEngine] ERROR: Match not initialized!\n";
        return;
    }
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║            BASKETBALL MANAGER - MATCH SIMULATION            ║\n";
    std::cout << "║                    48-MINUTE REGULATION                     ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::left << std::setw(25) << matchState->homeTeam->name 
              << " vs " << std::left << std::setw(25) << matchState->awayTeam->name << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    // Simulate 4 quarters
    for (int q = 1; q <= 4; ++q) {
        SimulateQuarter(q);
    }
    
    // Check for overtime (repeat if still tied)
    while (matchState->homeScore == matchState->awayScore) {
        std::cout << "\n═════════════════ OVERTIME ═════════════════\n";
        matchState->quarterNumber++;
        matchState->timeRemaining = 300;  // 5 minutes OT
        while (matchState->timeRemaining > 0) {
            SimulatePossession();
        }
    }
    
    matchState->matchComplete = true;
    
    // Final score display
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     FINAL SCORE                            ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::left << std::setw(25) << matchState->homeTeam->name 
              << " " << std::right << std::setw(3) << matchState->homeScore << "  ║\n";
    std::cout << "║  " << std::left << std::setw(25) << matchState->awayTeam->name 
              << " " << std::right << std::setw(3) << matchState->awayScore << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    // Show key stats
    std::cout << "MATCH STATISTICS:\n";
    std::cout << "  Possessions: " << matchState->possessionCount << "\n";
    std::cout << "  " << matchState->homeTeam->name << " Rebounds: " << matchState->homeRebounds << "\n";
    std::cout << "  " << matchState->awayTeam->name << " Rebounds: " << matchState->awayRebounds << "\n";
    std::cout << "  " << matchState->homeTeam->name << " Fouls: " << matchState->homeFouls << "\n";
    std::cout << "  " << matchState->awayTeam->name << " Fouls: " << matchState->awayFouls << "\n";
}

void MatchEngine::SimulateMatchWithSteps(int stepsPerUpdate) {
    if (!matchState) {
        std::cout << "[MatchEngine] ERROR: Match not initialized!\n";
        return;
    }
    
    // This method allows GUI to call SimulatePossession() stepsPerUpdate times per frame
    // The GUI can then render the results and handle user input between updates
    for (int i = 0; i < stepsPerUpdate; ++i) {
        if (matchState->matchComplete) break;
        
        // Check if quarter is complete
        if (matchState->timeRemaining <= 0) {
            if (matchState->quarterNumber < 4) {
                matchState->quarterNumber++;
                matchState->timeRemaining = 720;
            } else if (matchState->homeScore != matchState->awayScore) {
                matchState->matchComplete = true;
                break;
            } else {
                // Overtime (can stack multiple periods)
                matchState->quarterNumber++;
                matchState->timeRemaining = 300;
            }
        }
        
        SimulatePossession();
    }
}

float MatchEngine::CalculatePlaySuccessRate(const Player& player) {
    float baseRate = 0.50f;
    
    // Shooting is most important (max +5%)
    float shootingBonus = (player.attributes.shooting / 20.0f) * 0.05f;
    
    // Pace affects shot creation (max +2%)
    float paceBonus = (player.attributes.pace / 20.0f) * 0.02f;
    
    // Ball control (max +2%)
    float ballControlBonus = (player.attributes.ballControl / 20.0f) * 0.02f;
    
    // Technical IQ (max +1%)
    float technicalBonus = (player.attributes.technical / 20.0f) * 0.01f;
    
    return baseRate + shootingBonus + paceBonus + ballControlBonus + technicalBonus;
}

float MatchEngine::CalculateDefenseEffectiveness(const std::vector<std::shared_ptr<Player>>& defenders,
                                                 float defensiveIntensity) {
    if (defenders.empty()) return 0.15f;

    float totalDefense = 0.0f;
    for (auto& d : defenders) {
        totalDefense += static_cast<float>(d->attributes.defense);
    }
    float avgDefense = totalDefense / static_cast<float>(defenders.size());
    float base = 0.1f + (avgDefense / 20.0f) * 0.15f;
    base *= defensiveIntensity;
    return std::clamp(base, 0.08f, 0.35f);
}

std::string MatchEngine::GenerateCommentary() {
    if (!matchState || matchState->lastPlay.empty()) {
        return "Game in progress...";
    }
    return matchState->lastPlay;
}

std::string MatchEngine::GetPossessionCommentary() const {
    if (!matchState || matchState->lastPlay.empty()) {
        return "Game in progress...";
    }
    return matchState->lastPlay;
}

float MatchEngine::GetMatchProgress() const {
    if (!matchState) return 0.0f;
    
    // Calculate based on total game time
    // 48 minutes = 2880 seconds total
    int totalGameTime = 2880;
    float progress = static_cast<float>(matchState->timeElapsed) / totalGameTime;
    return std::min(1.0f, progress);
}

void MatchEngine::EnsureMinimumRoster(std::shared_ptr<Team> team) {
    if (!team) return;
    int missing = 12 - static_cast<int>(team->roster.size());
    if (missing <= 0) return;

    std::cout << "[MatchEngine] Padding roster for " << team->name << " with " << missing << " reserve players\n";
    for (int i = 0; i < missing; ++i) {
        auto reserve = std::make_shared<Player>();
        // Use teamId for stable unique player IDs and make reserve last-names readable
        std::string squad = team->name;
        for (auto &ch : squad) if (ch == ' ') ch = '_';
        reserve->playerId = team->teamId + "_reserve_" + std::to_string(i + 1);
        reserve->firstName = "Reserve";
        reserve->lastName = squad + "_R" + std::to_string(i + 1);
        reserve->position = static_cast<Position>(i % 5);
        reserve->height = 1.95f;
        reserve->weight = 90.0f;
        reserve->birthDate = "";
        reserve->jerseyNumber = 90 + i;
        reserve->attributes = {8, 8, 8, 8, 8, 8, 40, 45};
        reserve->yearInSchool = 1;
        reserve->international = false;
        reserve->draftEligible = false;
        reserve->nilValue = 0;
        reserve->active = true;
        reserve->injured = false;
        reserve->suspensionMatches = 0;
        reserve->gamesPlayed = 0;
        reserve->minutesPlayed = 0;
        reserve->pointsPerGame = 0;
        reserve->reboundsPerGame = 0;
        reserve->assistsPerGame = 0;
        reserve->onCourt = false;
        reserve->minutesPlayedThisGame = 0;
        reserve->fatigueLevel = 0.0f;
        reserve->pointsThisGame = 0;
        reserve->reboundsThisGame = 0;
        reserve->assistsThisGame = 0;
        reserve->foulsThisGame = 0;
        team->roster.push_back(reserve);
    }
}

void MatchEngine::HandlePauseMenu() {
    if (!matchState) return;
    
    bool paused = true;
    while (paused) {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                        ⏸️  GAME PAUSED                         ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  Q" << matchState->quarterNumber << " | " 
                  << FormatGameClock(matchState->timeRemaining) << " | " 
                  << matchState->homeTeam->name << " " << matchState->homeScore 
                  << " - " << matchState->awayScore << " " 
                  << matchState->awayTeam->name << std::right << std::setw(20) << "  ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║  1. Resume Game                                                ║\n";
        std::cout << "║  2. Change Speed (Current: x" << speedMultiplier << ")                                  ║\n";
        std::cout << "║  3. Call Timeout (Home: " << homeTimeoutsRemaining 
                  << " | Away: " << awayTimeoutsRemaining << ")                              ║\n";
        std::cout << "║  4. Make Substitution                                          ║\n";
        std::cout << "║  5. Show Player Stats                                          ║\n";
        std::cout << "║  6. Coach Callout (More Offense/Defense/Manage Time)           ║\n";
        std::cout << "║  7. Quit Game                                                  ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        std::cout << "Enter choice: ";
        
        int choice;
        std::cin >> choice;
        std::cin.ignore(1000, '\n');
        
        switch (choice) {
            case 1:
                paused = false;
                std::cout << "\n[RESUMING GAME...]\n\n";
                break;
                
            case 2: {
                std::cout << "\nSelect speed (1/2/3/4/6): ";
                int newSpeed;
                std::cin >> newSpeed;
                std::cin.ignore(1000, '\n');
                if (newSpeed == 1 || newSpeed == 2 || newSpeed == 3 || newSpeed == 4 || newSpeed == 6) {
                    speedMultiplier = newSpeed;
                    std::cout << "[Speed changed to x" << speedMultiplier << "]\n";
                } else {
                    std::cout << "[Invalid speed, keeping x" << speedMultiplier << "]\n";
                }
                break;
            }
                
            case 3: {
                std::cout << "\nCall timeout for: (1) " << matchState->homeTeam->name 
                          << " or (2) " << matchState->awayTeam->name << ": ";
                int teamChoice;
                std::cin >> teamChoice;
                std::cin.ignore(1000, '\n');
                if (teamChoice == 1 || teamChoice == 2) {
                    CallTimeout(teamChoice == 1);
                } else {
                    std::cout << "[Invalid choice]\n";
                }
                break;
            }
                
            case 4: {
                std::cout << "\nMake substitution for: (1) " << matchState->homeTeam->name 
                          << " or (2) " << matchState->awayTeam->name << ": ";
                int teamChoice;
                std::cin >> teamChoice;
                std::cin.ignore(1000, '\n');
                
                auto team = (teamChoice == 1) ? matchState->homeTeam : matchState->awayTeam;
                
                // Show current lineup
                std::cout << "\n[CURRENT LINEUP]\n";
                int idx = 1;
                for (auto& p : team->roster) {
                    if (p->onCourt) {
                        std::cout << "  " << idx++ << ". " << p->firstName << " " << p->lastName 
                                  << " (Fatigue: " << std::fixed << std::setprecision(0) 
                                  << (p->fatigueLevel * 100) << "%, Minutes: " 
                                  << std::setprecision(1) << p->minutesPlayedThisGame << ")\n";
                    }
                }
                
                // Show bench
                std::cout << "\n[BENCH]\n";
                idx = 6;
                for (auto& p : team->roster) {
                    if (!p->onCourt && !p->injured && p->active) {
                        std::cout << "  " << idx++ << ". " << p->firstName << " " << p->lastName 
                                  << " (Fatigue: " << std::fixed << std::setprecision(0) 
                                  << (p->fatigueLevel * 100) << "%)\n";
                    }
                }
                
                std::cout << "\nPlayer to substitute OUT (1-5): ";
                int outIdx;
                std::cin >> outIdx;
                std::cin.ignore(1000, '\n');
                
                std::cout << "Player to substitute IN (6+): ";
                int inIdx;
                std::cin >> inIdx;
                std::cin.ignore(1000, '\n');
                
                // Make the substitution
                if (outIdx >= 1 && outIdx <= 5 && inIdx >= 6 && inIdx < team->roster.size()) {
                    auto playerOut = team->roster[outIdx - 1];
                    auto playerIn = team->roster[inIdx - 1];
                    
                    if (playerOut->onCourt && !playerIn->onCourt && !playerIn->injured) {
                        playerOut->onCourt = false;
                        playerIn->onCourt = true;
                        std::cout << "\n[SUB] " << playerOut->lastName << " → " 
                                  << playerIn->lastName << "\n";
                    } else {
                        std::cout << "[Invalid substitution]\n";
                    }
                } else {
                    std::cout << "[Invalid player selection]\n";
                }
                break;
            }
                
            case 5: {
                std::cout << "\nShow stats for: (1) " << matchState->homeTeam->name 
                          << " or (2) " << matchState->awayTeam->name << ": ";
                int teamChoice;
                std::cin >> teamChoice;
                std::cin.ignore(1000, '\n');
                
                auto team = (teamChoice == 1) ? matchState->homeTeam : matchState->awayTeam;
                
                std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
                std::cout << "║  " << std::left << std::setw(60) << team->name << "  ║\n";
                std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
                std::cout << "║  Player                    PTS  REB  AST  MIN  FATIGUE         ║\n";
                std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
                
                for (auto& p : team->roster) {
                    if (p->minutesPlayedThisGame > 0 || p->onCourt) {
                        std::cout << "║  " << std::left << std::setw(20) 
                                  << (p->firstName.substr(0, 1) + ". " + p->lastName)
                                  << std::right << std::setw(5) << p->pointsThisGame
                                  << std::setw(5) << p->reboundsThisGame
                                  << std::setw(5) << p->assistsThisGame
                                  << std::setw(6) << std::fixed << std::setprecision(1) 
                                  << p->minutesPlayedThisGame
                                  << std::setw(8) << std::setprecision(0)
                                  << (p->fatigueLevel * 100) << "%"
                                  << (p->onCourt ? " *" : "  ")
                                  << std::left << std::setw(5) << "    ║\n";
                    }
                }
                std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
                std::cout << "  (* = currently on court)\n";
                break;
            }
            
            case 6: {
                std::cout << "\nCoach callout for: (1) " << matchState->homeTeam->name 
                          << " or (2) " << matchState->awayTeam->name << ": ";
                int teamChoice;
                std::cin >> teamChoice;
                std::cin.ignore(1000, '\n');
                
                if (teamChoice != 1 && teamChoice != 2) {
                    std::cout << "[Invalid choice]\n";
                    break;
                }
                
                bool isHome = (teamChoice == 1);
                std::string teamName = isHome ? matchState->homeTeam->name : matchState->awayTeam->name;
                
                std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
                std::cout << "║  " << std::left << std::setw(60) << ("Coach - " + teamName) << "  ║\n";
                std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
                std::cout << "║  1. Ask team to be MORE OFFENSIVE                             ║\n";
                std::cout << "║  2. Ask team to be MORE DEFENSIVE                             ║\n";
                std::cout << "║  3. Ask team to MANAGE TIME (slow down pace)                 ║\n";
                std::cout << "║  4. Ask team to KEEP GOING (maintain strategy)               ║\n";
                std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
                std::cout << "Enter callout choice: ";
                
                int calloutChoice;
                std::cin >> calloutChoice;
                std::cin.ignore(1000, '\n');
                
                CoachingCallout callout;
                switch (calloutChoice) {
                    case 1: callout = CoachingCallout::MORE_OFFENSIVE; break;
                    case 2: callout = CoachingCallout::MORE_DEFENSIVE; break;
                    case 3: callout = CoachingCallout::MANAGE_TIME; break;
                    case 4: callout = CoachingCallout::KEEP_GOING; break;
                    default:
                        std::cout << "[Invalid callout choice]\n";
                        break;
                }
                
                if (calloutChoice >= 1 && calloutChoice <= 4) {
                    ApplyCoachingCallout(isHome, callout);
                }
                break;
            }
                
            case 7:
                std::cout << "\n[QUITTING GAME...]\n";
                matchState->matchComplete = true;
                matchState->timeRemaining = 0;
                paused = false;
                break;
                
            default:
                std::cout << "[Invalid choice]\n";
                break;
        }
    }
}

void MatchEngine::CallTimeout(bool isHomeTeam) {
    if (!matchState) return;

    // UI path: the request is granted at the next dead ball and handled as a break
    if (headless) {
        RequestTimeout(isHomeTeam, true);
        return;
    }

    int& timeouts = isHomeTeam ? homeTimeoutsRemaining : awayTimeoutsRemaining;
    std::string teamName = isHomeTeam ? matchState->homeTeam->name : matchState->awayTeam->name;
    
    if (timeouts <= 0) {
        std::cout << "\n[ERROR] " << teamName << " has no timeouts remaining!\n";
        return;
    }
    
    timeouts--;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        ⏱️  TIMEOUT CALLED                       ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::left << std::setw(60) << (teamName + " calls timeout") << "  ║\n";
    std::cout << "║  " << std::left << std::setw(60) 
              << ("Timeouts remaining: " + std::to_string(timeouts)) << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // 30-second timeout - allow substitutions and fatigue recovery
    std::cout << "\n[30-second timeout - Players recover slightly...]\n";
    
    auto team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;
    for (auto& p : team->roster) {
        if (p->onCourt) {
            p->fatigueLevel = std::max(0.0f, p->fatigueLevel - 0.05f);  // Small recovery
        }
    }
    
    // Offer substitution opportunity
    std::cout << "\nMake substitutions during timeout? (y/n): ";
    std::string response;
    std::cin >> response;
    std::cin.ignore(1000, '\n');
    
    if (response == "y" || response == "Y") {
        // Show current lineup
        std::cout << "\n[CURRENT LINEUP]\n";
        int idx = 1;
        for (auto& p : team->roster) {
            if (p->onCourt) {
                std::cout << "  " << idx++ << ". " << p->firstName << " " << p->lastName 
                          << " (Fatigue: " << std::fixed << std::setprecision(0) 
                          << (p->fatigueLevel * 100) << "%, Minutes: " 
                          << std::setprecision(1) << p->minutesPlayedThisGame << ")\n";
            }
        }
        
        // Show bench
        std::cout << "\n[BENCH]\n";
        idx = 6;
        for (auto& p : team->roster) {
            if (!p->onCourt && !p->injured && p->active) {
                std::cout << "  " << idx++ << ". " << p->firstName << " " << p->lastName 
                          << " (Fatigue: " << std::fixed << std::setprecision(0) 
                          << (p->fatigueLevel * 100) << "%)\n";
            }
        }
        
        std::cout << "\nPlayer to substitute OUT (1-5, 0 to skip): ";
        int outIdx;
        std::cin >> outIdx;
        std::cin.ignore(1000, '\n');
        
        if (outIdx > 0 && outIdx <= 5) {
            std::cout << "Player to substitute IN (6+): ";
            int inIdx;
            std::cin >> inIdx;
            std::cin.ignore(1000, '\n');
            
            if (inIdx >= 6 && inIdx <= team->roster.size()) {
                auto playerOut = team->roster[outIdx - 1];
                auto playerIn = team->roster[inIdx - 1];
                
                if (playerOut->onCourt && !playerIn->onCourt && !playerIn->injured) {
                    playerOut->onCourt = false;
                    playerIn->onCourt = true;
                    std::cout << "\n[SUB] " << playerOut->lastName << " → " 
                              << playerIn->lastName << "\n";
                }
            }
        }
    }
    
    std::cout << "\n[Timeout complete - Resuming play...]\n\n";
    if (!headless && speedMultiplier > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500 / speedMultiplier));
    }
}

void MatchEngine::ApplyCoachingCallout(bool isHomeTeam, CoachingCallout callout) {
    if (!matchState) return;
    
    auto team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;
    std::string teamName = team->name;
    std::string calloutText;
    
    // Display coaching callout message
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    🏀 COACHING CALLOUT 🏀                      ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════╣\n";
    
    // Set momentum duration and apply tactical changes
    if (isHomeTeam) {
        homeCoachingMomentum = COACHING_MOMENTUM_DURATION;
        homeLastCallout = callout;
    } else {
        awayCoachingMomentum = COACHING_MOMENTUM_DURATION;
        awayLastCallout = callout;
    }
    
    auto offenseStrategy = isHomeTeam ? tacticalManager->GetHomeOffensiveStrategy() 
                                       : tacticalManager->GetAwayOffensiveStrategy();
    auto defenseStrategy = isHomeTeam ? tacticalManager->GetHomeDefensiveStrategy() 
                                       : tacticalManager->GetAwayDefensiveStrategy();
    
    switch (callout) {
        case CoachingCallout::MORE_OFFENSIVE:
            calloutText = "Asking team to play more OFFENSIVELY!";
            if (isHomeTeam) {
                tacticalManager->SetHomeOffensiveStrategy(OffensiveStrategy::THREE_POINT_HEAVY);
                tacticalManager->SetHomeDefensiveStrategy(DefensiveStrategy::BALANCED_DEFENSE);
            } else {
                tacticalManager->SetAwayOffensiveStrategy(OffensiveStrategy::THREE_POINT_HEAVY);
                tacticalManager->SetAwayDefensiveStrategy(DefensiveStrategy::BALANCED_DEFENSE);
            }
            break;
            
        case CoachingCallout::MORE_DEFENSIVE:
            calloutText = "Asking team to focus on DEFENSE!";
            if (isHomeTeam) {
                tacticalManager->SetHomeOffensiveStrategy(OffensiveStrategy::BALANCED);
                tacticalManager->SetHomeDefensiveStrategy(DefensiveStrategy::PRESS);
            } else {
                tacticalManager->SetAwayOffensiveStrategy(OffensiveStrategy::BALANCED);
                tacticalManager->SetAwayDefensiveStrategy(DefensiveStrategy::PRESS);
            }
            break;
            
        case CoachingCallout::MANAGE_TIME:
            calloutText = "Asking team to MANAGE THE CLOCK!";
            if (isHomeTeam) {
                tacticalManager->SetHomeOffensiveStrategy(OffensiveStrategy::INSIDE_FOCUSED);
                tacticalManager->SetHomeDefensiveStrategy(DefensiveStrategy::ZONE_2_3);
            } else {
                tacticalManager->SetAwayOffensiveStrategy(OffensiveStrategy::INSIDE_FOCUSED);
                tacticalManager->SetAwayDefensiveStrategy(DefensiveStrategy::ZONE_2_3);
            }
            break;
            
        case CoachingCallout::KEEP_GOING:
            calloutText = "Reminding team to KEEP GOING and STAY FOCUSED!";
            // Keep current strategies, just give morale boost
            break;
    }
    
    std::cout << "║  " << std::left << std::setw(60) << (teamName + " - " + calloutText) << "  ║\n";
    std::cout << "║  " << std::left << std::setw(60) 
              << ("Effect duration: " + std::to_string(COACHING_MOMENTUM_DURATION) + " possessions") 
              << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    if (!headless && speedMultiplier > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / speedMultiplier));
    }
    EmitFeed(teamName + " coach: " + calloutText);
}

void MatchEngine::UpdateSpatialState(const std::shared_ptr<Player>& ballHandler,
                                     const std::shared_ptr<Player>& primaryDefender,
                                     const std::string& actionPhase) {
    if (!matchState || !matchState->homeTeam || !matchState->awayTeam) return;

    matchState->courtPlayers.clear();

    // Standard NBA/NCAA court dimensions: 94 feet long by 50 feet wide
    // Left basket: (5.25, 25.0), Right basket: (88.75, 25.0)
    // Home attacks Right basket (x = 88.75) if homeTeamPossession, Away attacks Left basket (x = 5.25)
    bool homeHasBall = matchState->homeTeamPossession;
    float targetBasketX = homeHasBall ? 88.75f : 5.25f;
    float basketY = 25.0f;

    auto assignTeamCoordinates = [&](const std::vector<std::shared_ptr<Player>>& roster,
                                     bool isHome,
                                     bool isOffense) {
        std::vector<std::shared_ptr<Player>> onCourt;
        for (const auto& p : roster) {
            if (p && p->onCourt) onCourt.push_back(p);
        }

        // Base court anchor depending on offense or defense
        // Offense sets up around three-point line of target basket (~23.75 ft radius)
        float basketDirection = isHome ? 1.0f : -1.0f;
        float baseBackcourtX = isHome ? 25.0f : 69.0f;
        float baseFrontcourtX = isHome ? 68.0f : 26.0f;

        for (size_t i = 0; i < onCourt.size() && i < 5; ++i) {
            auto player = onCourt[i];
            CourtPlayerState cp;
            cp.playerId = player->playerId;
            cp.name = player->firstName + " " + player->lastName;
            cp.teamId = isHome ? matchState->homeTeam->teamId : matchState->awayTeam->teamId;
            cp.isHome = isHome;
            cp.position = player->position;
            cp.jerseyNumber = player->jerseyNumber;
            cp.hasBall = (ballHandler && player->playerId == ballHandler->playerId);

            // Compute realistic positions based on role & offense/defense
            float posX = 47.0f; // midcourt default
            float posY = 25.0f;

            if (actionPhase == "tipoff") {
                // Circle around center circle (47.0, 25.0)
                float angle = (i * 72.0f) * 3.14159f / 180.0f;
                float radius = isOffense ? 6.0f : 9.0f;
                posX = 47.0f + (isHome ? -1.0f : 1.0f) * std::abs(std::cos(angle) * radius);
                posY = 25.0f + std::sin(angle) * radius;
                cp.action = "idle";
            } else if (isOffense) {
                // Set up 1-2-2 halfcourt offense towards target basket
                float hoopX = targetBasketX;
                float dir = (hoopX > 47.0f) ? -1.0f : 1.0f; // vector from hoop back towards perimeter

                switch (player->position) {
                    case Position::PG: // Top of the key (around 25-28 feet out)
                        posX = hoopX + dir * 26.0f;
                        posY = 25.0f + (RandomFloat() - 0.5f) * 4.0f;
                        break;
                    case Position::SG: // Right wing
                        posX = hoopX + dir * 22.0f;
                        posY = 10.0f + (RandomFloat() - 0.5f) * 4.0f;
                        break;
                    case Position::SF: // Left wing
                        posX = hoopX + dir * 22.0f;
                        posY = 40.0f + (RandomFloat() - 0.5f) * 4.0f;
                        break;
                    case Position::PF: // High post / short corner
                        posX = hoopX + dir * 14.0f;
                        posY = 15.0f + (RandomFloat() - 0.5f) * 4.0f;
                        break;
                    case Position::C: // Low post / paint
                        posX = hoopX + dir * 8.0f;
                        posY = 28.0f + (RandomFloat() - 0.5f) * 4.0f;
                        break;
                }

                // If this player is shooting or driving
                if (cp.hasBall) {
                    if (actionPhase == "shot" || actionPhase == "shot_make" || actionPhase == "three_make" || actionPhase == "block") {
                        cp.action = "shoot";
                    } else if (actionPhase == "foul") {
                        cp.action = "foul";
                    } else if (actionPhase == "turnover" || actionPhase == "steal") {
                        cp.action = "turnover";
                    } else {
                        cp.action = "dribble";
                    }
                } else {
                    cp.action = (actionPhase == "rebound" || actionPhase == "miss") ? "rebound" : "idle";
                }
            } else {
                // Defense: match up between offensive players and hoop
                float hoopX = targetBasketX;
                float dir = (hoopX > 47.0f) ? -1.0f : 1.0f;

                switch (player->position) {
                    case Position::PG: // Defend point
                        posX = hoopX + dir * 22.0f;
                        posY = 25.0f;
                        break;
                    case Position::SG: // Defend right wing
                        posX = hoopX + dir * 18.0f;
                        posY = 13.0f;
                        break;
                    case Position::SF: // Defend left wing
                        posX = hoopX + dir * 18.0f;
                        posY = 37.0f;
                        break;
                    case Position::PF: // Low post defense
                        posX = hoopX + dir * 11.0f;
                        posY = 17.0f;
                        break;
                    case Position::C: // Rim protector
                        posX = hoopX + dir * 5.0f;
                        posY = 25.0f;
                        break;
                }

                if (primaryDefender && player->playerId == primaryDefender->playerId) {
                    if (actionPhase == "block") {
                        cp.action = "block";
                    } else if (actionPhase == "steal") {
                        cp.action = "steal";
                    } else {
                        cp.action = "defend";
                    }
                } else {
                    cp.action = (actionPhase == "miss" || actionPhase == "rebound") ? "rebound" : "defend";
                }
            }

            // Clamp inside court bounds
            cp.x = std::clamp(posX, 2.0f, 92.0f);
            cp.y = std::clamp(posY, 2.0f, 48.0f);

            matchState->courtPlayers.push_back(cp);
        }
    };

    assignTeamCoordinates(matchState->homeTeam->roster, true, homeHasBall);
    assignTeamCoordinates(matchState->awayTeam->roster, false, !homeHasBall);

    // Update ball state
    matchState->ball.phase = actionPhase;
    if (ballHandler) {
        matchState->ball.holderPlayerId = ballHandler->playerId;
        // Locate ballHandler in courtPlayers
        for (const auto& cp : matchState->courtPlayers) {
            if (cp.playerId == ballHandler->playerId) {
                if (actionPhase == "shot" || actionPhase == "shot_make" || actionPhase == "three_make" || actionPhase == "miss") {
                    // Ball in flight towards target basket
                    matchState->ball.x = (cp.x + targetBasketX) * 0.5f;
                    matchState->ball.y = (cp.y + basketY) * 0.5f;
                    matchState->ball.z = 11.0f; // arc height
                    matchState->ball.inFlight = true;
                } else {
                    matchState->ball.x = cp.x;
                    matchState->ball.y = cp.y;
                    matchState->ball.z = 3.5f; // waist height
                    matchState->ball.inFlight = false;
                }
                break;
            }
        }
    } else {
        matchState->ball.holderPlayerId = "";
        matchState->ball.x = 47.0f;
        matchState->ball.y = 25.0f;
        matchState->ball.z = 8.0f;
        matchState->ball.inFlight = true;
    }
}

// ============================================================================
// Live second-by-second playback
//
// A planned possession is turned into a queue of "beats" (inbound, advance, passes, drive, shot,
// rebound ...). StepSecond() plays one game second of the current beat: players walk towards the
// beat's destinations, the ball follows its holder or flies to the next one, and the clock runs.
// Score and stats only change when the beat that decides them completes.
// ============================================================================

namespace {

constexpr float kLeftHoopX = 5.25f;
constexpr float kRightHoopX = 88.75f;
constexpr float kHoopY = 25.0f;

struct Slot {
    float dx;  // feet from the attacked hoop towards midcourt
    float y;
};

constexpr Slot kMotionSlots[5] = {{27, 25}, {22, 9}, {22, 41}, {13, 19}, {7, 29}};
constexpr Slot kFourOutSlots[5] = {{27, 25}, {23, 8}, {23, 42}, {4, 3}, {8, 27}};
constexpr Slot kInsideSlots[5] = {{22, 25}, {17, 12}, {17, 38}, {11, 25}, {5, 22}};
constexpr Slot kZone23[5] = {{16, 19}, {16, 31}, {9, 10}, {9, 40}, {4, 25}};
constexpr Slot kZone32[5] = {{17, 12}, {17, 25}, {17, 38}, {7, 18}, {7, 32}};

using Moves = std::unordered_map<std::string, CourtPoint>;

float Clampf(float v, float lo, float hi) {
    return std::min(hi, std::max(lo, v));
}

CourtPoint ClampToCourt(CourtPoint p) {
    return CourtPoint{Clampf(p.x, 1.5f, 92.5f), Clampf(p.y, 1.5f, 48.5f)};
}

float Jit(float range) {
    return RandomFloatRange(-range, range);
}

const char* PickText(std::initializer_list<const char*> options) {
    const int index = RandomInt(0, static_cast<int>(options.size()) - 1);
    return *(options.begin() + index);
}

std::string Fill(const char* tpl, const std::string& a, const std::string& b = std::string()) {
    std::string s(tpl);
    auto replaceAll = [&s](const std::string& token, const std::string& value) {
        size_t pos = 0;
        while ((pos = s.find(token, pos)) != std::string::npos) {
            s.replace(pos, token.size(), value);
            pos += value.size();
        }
    };
    replaceAll("{A}", a);
    replaceAll("{B}", b);
    return s;
}

const Slot* OffenseSlots(OffensiveStrategy strategy) {
    switch (strategy) {
        case OffensiveStrategy::THREE_POINT_HEAVY:
        case OffensiveStrategy::PACE_AND_SPACE:
            return kFourOutSlots;
        case OffensiveStrategy::INSIDE_FOCUSED:
            return kInsideSlots;
        default:
            return kMotionSlots;
    }
}

std::string PassText(const std::shared_ptr<Player>& from, const std::shared_ptr<Player>& to) {
    const bool inside = to->position == Position::C || to->position == Position::PF;
    if (inside && RandomFloat() < 0.6f) {
        return Fill(PickText({"{A} feeds {B} inside.", "{A} looks inside to {B}.", "{A} drops it to {B} in the post."}),
                    from->lastName, to->lastName);
    }
    return Fill(PickText({"{A} swings it to {B}.", "{A} finds {B} on the wing.", "{A} kicks it out to {B}.",
                          "{A} moves the ball to {B}.", "{A} hits {B} on the move."}),
                from->lastName, to->lastName);
}

} // namespace

void MatchEngine::RebuildCourtRoster() {
    std::vector<CourtPlayerState> next;
    actorById.clear();

    auto addTeam = [&](const std::shared_ptr<Team>& team, bool isHome) {
        int count = 0;
        for (const auto& p : team->roster) {
            if (!p || !p->onCourt) continue;
            if (count++ >= 5) break;

            CourtPlayerState cp;
            bool found = false;
            for (const auto& previous : matchState->courtPlayers) {
                if (previous.playerId == p->playerId) {
                    cp = previous;
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Substitutes walk in from the sideline
                cp.x = 47.0f;
                cp.y = isHome ? 1.5f : 48.5f;
                cp.action = "idle";
            }
            cp.playerId = p->playerId;
            cp.name = p->firstName + " " + p->lastName;
            cp.teamId = team->teamId;
            cp.isHome = isHome;
            cp.position = p->position;
            cp.jerseyNumber = p->jerseyNumber;
            cp.hasBall = false;
            next.push_back(cp);
            actorById[p->playerId] = p;
        }
    };

    addTeam(matchState->homeTeam, true);
    addTeam(matchState->awayTeam, false);
    matchState->courtPlayers = std::move(next);
}

void MatchEngine::BuildScript(const PossessionPlan& plan) {
    beatQueue.clear();
    beatActive = false;
    RebuildCourtRoster();

    const bool homeOff = plan.homeOffense;
    const float hoopX = homeOff ? kRightHoopX : kLeftHoopX;
    const float dir = homeOff ? -1.0f : 1.0f;             // from the attacked hoop towards midcourt
    const float ownHoopX = homeOff ? kLeftHoopX : kRightHoopX;
    const float ownDir = -dir;                            // from the own hoop towards midcourt
    const CourtPoint hoop{hoopX, kHoopY};
    const PossessionEnd end = plan.end;

    const auto& off = plan.onCourtOffense;
    const auto offStrategy = homeOff ? tacticalManager->GetHomeOffensiveStrategy()
                                     : tacticalManager->GetAwayOffensiveStrategy();
    const auto defStrategy = homeOff ? tacticalManager->GetAwayDefensiveStrategy()
                                     : tacticalManager->GetHomeDefensiveStrategy();
    const Slot* slots = OffenseSlots(offStrategy);

    // A called play (when it belongs to the scheme) picks the floor spacing; the hub player is the featured one
    const std::string& calledPlay = offensePlay[homeOff ? 0 : 1];
    for (const auto& pl : GetPlaybook()) {
        if (pl.id != calledPlay || pl.scheme != offStrategy) continue;
        if (pl.id == "five_out" || pl.id == "transition_threes") slots = kFourOutSlots;
        else if (pl.id == "post_split" || pl.id == "dribble_drive") slots = kInsideSlots;
    }
    const bool pnr = (offStrategy == OffensiveStrategy::PICK_AND_ROLL);
    const std::string& hubId = offenseTarget[homeOff ? 0 : 1];
    std::string screenerId;   // big man who screens, then rolls to the rim
    int screenPhase = 0;
    const bool zone = defStrategy == DefensiveStrategy::ZONE_2_3 || defStrategy == DefensiveStrategy::ZONE_3_2;
    const bool pressure = defStrategy == DefensiveStrategy::PRESS || defStrategy == DefensiveStrategy::MAN_TO_MAN;

    // Where everybody is now, and where the beats queued so far will leave them
    std::unordered_map<std::string, CourtPoint> here;
    for (const auto& cp : matchState->courtPlayers) here[cp.playerId] = CourtPoint{cp.x, cp.y};
    Moves planned;
    auto posOf = [&](const std::string& id) {
        auto it = planned.find(id);
        if (it != planned.end()) return it->second;
        auto h = here.find(id);
        return h != here.end() ? h->second : CourtPoint{47.0f, 25.0f};
    };

    // Floor spots by role and defensive matchups
    const auto offOrder = SortByRole(plan.onCourtOffense);
    const auto defOrder = SortByRole(plan.onCourtDefense);
    Moves spot;
    std::unordered_map<std::string, std::string> guardOf;  // offensive player id -> his defender
    for (size_t k = 0; k < offOrder.size() && k < 5; ++k) {
        spot[offOrder[k]->playerId] = CourtPoint{hoopX + dir * slots[k].dx, slots[k].y};
        if (k < defOrder.size()) guardOf[offOrder[k]->playerId] = defOrder[k]->playerId;
    }

    auto placeDefense = [&](Moves& dest, const std::string& holderId) {
        if (zone) {
            const Slot* z = (defStrategy == DefensiveStrategy::ZONE_3_2) ? kZone32 : kZone23;
            float holderY = kHoopY;
            auto holder = dest.find(holderId);
            if (holder != dest.end()) holderY = holder->second.y;
            const float shift = 0.35f * (holderY - kHoopY);
            for (size_t k = 0; k < defOrder.size() && k < 5; ++k) {
                dest[defOrder[k]->playerId] = ClampToCourt(CourtPoint{hoopX + dir * z[k].dx, z[k].y + shift});
            }
            return;
        }
        for (const auto& o : off) {
            auto guard = guardOf.find(o->playerId);
            if (guard == guardOf.end()) continue;
            auto where = dest.find(o->playerId);
            const CourtPoint op = (where != dest.end()) ? where->second : posOf(o->playerId);
            const float tx = hoop.x - op.x;
            const float ty = hoop.y - op.y;
            const float dist = std::max(0.1f, std::hypot(tx, ty));
            const bool onBall = (o->playerId == holderId);
            float gap = onBall ? (pressure ? 2.0f : 3.0f) : (pressure ? 3.5f : 4.5f + 0.08f * dist);
            gap = std::min(gap, std::max(0.0f, dist - 2.0f));
            dest[guard->second] = ClampToCourt(CourtPoint{op.x + tx / dist * gap, op.y + ty / dist * gap});
        }
    };

    auto setSpots = [&](Moves& dest, float jitter) {
        for (const auto& o : off) {
            auto it = spot.find(o->playerId);
            if (it == spot.end()) continue;
            dest[o->playerId] = ClampToCourt(CourtPoint{it->second.x + Jit(jitter), it->second.y + Jit(jitter)});
        }
    };

    auto rimPoint = [&](float minFt, float maxFt, float yRange) {
        return ClampToCourt(CourtPoint{hoopX + dir * RandomFloatRange(minFt, maxFt), kHoopY + Jit(yRange)});
    };

    // Half-court movement: spacing shifts (two players trade spots for good), sometimes a cutter slices to the rim
    auto setMoves = [&](const std::string& fromId, const std::string& toId, bool allowCut) {
        std::vector<std::string> free;
        for (const auto& o : off) {
            if (o->playerId != fromId && o->playerId != toId) free.push_back(o->playerId);
        }
        if (free.size() >= 2 && RandomFloat() < 0.45f) {
            const int a = RandomInt(0, static_cast<int>(free.size()) - 1);
            int b = RandomInt(0, static_cast<int>(free.size()) - 2);
            if (b >= a) ++b;
            if (spot.count(free[static_cast<size_t>(a)]) && spot.count(free[static_cast<size_t>(b)])) {
                std::swap(spot[free[static_cast<size_t>(a)]], spot[free[static_cast<size_t>(b)]]);
            }
        }

        Moves mv;
        setSpots(mv, 2.0f);
        if (allowCut && !free.empty() && RandomFloat() < 0.4f) {
            mv[free[static_cast<size_t>(RandomInt(0, static_cast<int>(free.size()) - 1))]] = rimPoint(3.0f, 6.0f, 8.0f);
        }
        if (!screenerId.empty() && screenerId != fromId && screenerId != toId) {
            if (screenPhase++ % 2 == 0) {
                // Screen on the ball: stand between the ball handler and the hoop
                const CourtPoint hp = posOf(toId);
                const float tx = hoop.x - hp.x;
                const float ty = hoop.y - hp.y;
                const float dist = std::max(0.1f, std::hypot(tx, ty));
                mv[screenerId] = ClampToCourt(CourtPoint{hp.x + tx / dist * 3.0f, hp.y + ty / dist * 3.0f});
            } else {
                mv[screenerId] = rimPoint(3.0f, 6.0f, 3.0f);   // roll to the rim
            }
        }
        placeDefense(mv, toId);
        return mv;
    };

    auto makeBeat = [&](const char* phase, int seconds, bool clockRuns, float speed) {
        PossessionBeat b;
        b.phase = phase;
        b.seconds = std::max(1, seconds);
        b.clockRuns = clockRuns;
        b.speedFactor = speed;
        b.hoop = hoop;
        return b;
    };

    auto pushBeat = [&](PossessionBeat&& b) {
        for (const auto& kv : b.moves) planned[kv.first] = kv.second;
        beatQueue.push_back(std::move(b));
    };

    // ----- Who touches the ball, and how much time there is for each phase -----
    const std::shared_ptr<Player> finalHolder = (end == PossessionEnd::Steal) ? plan.passer : plan.primary;
    const std::shared_ptr<Player> forced = plan.entryHolder;  // already has the ball when the possession starts
    const std::string& finalId = finalHolder->playerId;

    const bool reboundEnd = (end == PossessionEnd::DefensiveRebound || end == PossessionEnd::OffensiveRebound);
    const bool shotEnd = (end == PossessionEnd::MadeShot || end == PossessionEnd::ShootingFoul ||
                          end == PossessionEnd::Block || reboundEnd);
    const bool merged = reboundEnd && plan.duration < 2;  // not enough clock for a separate rebound beat
    const int tail = (reboundEnd && !merged) ? 2 : 1;
    const int lead = std::max(0, plan.duration - tail);

    const bool inboundBeats = (plan.entry == PossessionEntry::Inbound) && lead >= 1;
    int afterEntry = lead - (inboundBeats ? 1 : 0);

    const bool outlet = plan.entry == PossessionEntry::Outlet && !plan.fastBreak && forced && forced != finalHolder &&
                        afterEntry >= 2;
    if (outlet) afterEntry -= 1;

    int advance = 0;
    if (plan.entry != PossessionEntry::PutBack && afterEntry >= 1) {
        if (plan.fastBreak) {
            // A break is one long sprint; keep a second back only when the ball has to be passed ahead first
            const bool needsPass = forced && forced != finalHolder && afterEntry >= 2;
            advance = afterEntry - (needsPass ? 1 : 0);
        } else {
            advance = std::min(RandomInt(3, 5), afterEntry);
        }
    }
    const int rem = afterEntry - advance;

    const bool drive = shotEnd && !plan.isThree && !plan.fastBreak && rem >= 3 &&
                       RandomFloat() < (finalHolder->attributes.ballControl >= 11 ? 0.6f : 0.25f);
    const int driveSecs = drive ? 2 : 0;
    const int mMax = std::max(0, rem - driveSecs);

    // More ball movement when there is more time: roughly one pass per three seconds of half-court play
    int desired;
    if (end == PossessionEnd::Steal) {
        desired = RandomInt(0, std::max(1, rem / 4));
    } else if (plan.fastBreak) {
        desired = RandomInt(0, 1);
    } else {
        desired = std::min(6, RandomInt(std::max(1, rem / 4), std::max(1, (rem * 2) / 5)));
    }
    int mExtra = std::min(desired, mMax);
    if (!outlet && forced && forced != finalHolder && mExtra == 0 && mMax >= 1) mExtra = 1;
    if (forced && forced == finalHolder) mExtra = 0;  // he already has it
    const int m = (outlet ? 1 : 0) + mExtra;          // completed passes before the final action

    // ----- Chain of ball holders: holders[0] starts, holders[m] makes the final action -----
    auto bestHandlerExcluding = [&](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
        std::shared_ptr<Player> best;
        for (const auto& p : off) {
            if (p == a || p == b) continue;
            if (!best || p->attributes.ballControl > best->attributes.ballControl) best = p;
        }
        return best;
    };

    std::vector<std::shared_ptr<Player>> holders(static_cast<size_t>(m) + 1, finalHolder);
    if (m >= 1) {
        std::shared_ptr<Player> start = (forced && forced != finalHolder) ? forced : bestHandlerExcluding(finalHolder, nullptr);
        holders[0] = start ? start : finalHolder;
        for (int k = 1; k < m; ++k) {
            std::shared_ptr<Player> pick;
            if (k == 1 && outlet) {
                pick = bestHandlerExcluding(holders[0], finalHolder);
            } else {
                std::vector<std::shared_ptr<Player>> options;
                for (const auto& p : off) {
                    if (p != holders[static_cast<size_t>(k) - 1] && p != finalHolder) options.push_back(p);
                }
                if (!options.empty()) pick = options[RandomInt(0, static_cast<int>(options.size()) - 1)];
            }
            holders[static_cast<size_t>(k)] = pick ? pick : holders[static_cast<size_t>(k) - 1];
        }
    }
    // Play through a player: the ball usually runs through the hub before the shot
    if (offStrategy == OffensiveStrategy::PLAY_THROUGH_PLAYER && !hubId.empty() && m >= 2 && RandomFloat() < 0.6f) {
        for (const auto& p : off) {
            if (p->playerId == hubId && p != finalHolder && holders[static_cast<size_t>(m) - 2] != p) {
                holders[static_cast<size_t>(m) - 1] = p;
            }
        }
    }

    // Pick and roll: the strongest big who is not carrying the ball sets the screens
    if (pnr) {
        std::shared_ptr<Player> screener;
        for (const auto& p : off) {
            if (p == finalHolder || p == holders[0]) continue;
            if (!screener || p->attributes.physical > screener->attributes.physical) screener = p;
        }
        if (screener) screenerId = screener->playerId;
    }

    const size_t startIdx = outlet ? 1 : 0;
    const std::shared_ptr<Player> assister = (end == PossessionEnd::MadeShot && plan.assisted && m >= 1)
        ? holders[static_cast<size_t>(m) - 1] : nullptr;

    const PossessionPlan planCopy = plan;  // captured by the completion handlers below

    // ----- Inbound (clock stopped, then the inbound pass) -----
    if (inboundBeats) {
        std::vector<std::shared_ptr<Player>> options;
        for (const auto& p : off) {
            if (p != holders[0]) options.push_back(p);
        }
        std::shared_ptr<Player> inbounder = options.empty() ? holders[0] : PickWeighted(options, [](const std::shared_ptr<Player>& p) {
            return (p->position == Position::C || p->position == Position::PF) ? 3.0f : 1.0f;
        });

        PossessionBeat reset = makeBeat("inbound", 2, false, 1.0f);
        reset.moves[inbounder->playerId] = CourtPoint{ownHoopX - ownDir * 4.5f, kHoopY};
        reset.moves[holders[0]->playerId] = ClampToCourt(CourtPoint{ownHoopX + ownDir * 12.0f, kHoopY + Jit(6.0f)});
        int lane = 0;
        for (const auto& o : off) {
            if (o == inbounder || o == holders[0]) continue;
            reset.moves[o->playerId] = ClampToCourt(CourtPoint{ownHoopX + ownDir * (16.0f + 6.0f * lane),
                                                               12.0f + 13.0f * (lane % 3) + Jit(2.0f)});
            ++lane;
        }
        placeDefense(reset.moves, holders[0]->playerId);
        reset.ballTo = inbounder->playerId;
        pushBeat(std::move(reset));

        PossessionBeat inPass = makeBeat("pass", 1, true, 0.8f);
        setSpots(inPass.moves, 1.0f);
        placeDefense(inPass.moves, holders[0]->playerId);
        inPass.ballFrom = inbounder->playerId;
        inPass.ballTo = holders[0]->playerId;
        inPass.startText = Fill("{A} inbounds to {B}.", inbounder->lastName, holders[0]->lastName);
        pushBeat(std::move(inPass));
    }

    // ----- Outlet pass to the ball handler after a rebound -----
    if (outlet) {
        PossessionBeat ob = makeBeat("pass", 1, true, 0.9f);
        setSpots(ob.moves, 1.0f);
        placeDefense(ob.moves, holders[1]->playerId);
        ob.ballFrom = holders[0]->playerId;
        ob.ballTo = holders[1]->playerId;
        ob.startText = Fill("{A} outlets to {B}.", holders[0]->lastName, holders[1]->lastName);
        pushBeat(std::move(ob));
    }

    // ----- Bringing the ball up the floor, or a fast break -----
    const std::shared_ptr<Player>& carrier = holders[startIdx];
    if (advance >= 1) {
        PossessionBeat adv = makeBeat("advance", advance, true, plan.fastBreak ? 1.0f : 0.8f);
        if (plan.fastBreak) {
            // Handler down the middle, the quickest teammates fill the wings, the rest trail
            std::vector<std::shared_ptr<Player>> others;
            for (const auto& p : off) {
                if (p != carrier) others.push_back(p);
            }
            std::sort(others.begin(), others.end(), [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
                return a->attributes.pace > b->attributes.pace;
            });
            const CourtPoint lanes[4] = {{hoopX + dir * 11.0f, 9.0f}, {hoopX + dir * 11.0f, 41.0f},
                                         {hoopX + dir * 21.0f, 18.0f}, {hoopX + dir * 23.0f, 32.0f}};
            adv.moves[carrier->playerId] = ClampToCourt(CourtPoint{hoopX + dir * 14.0f, kHoopY});
            for (size_t k = 0; k < others.size() && k < 4; ++k) {
                adv.moves[others[k]->playerId] = ClampToCourt(CourtPoint{lanes[k].x + Jit(1.5f), lanes[k].y + Jit(2.0f)});
            }
            adv.startText = Fill(PickText({"{A} pushes it up the floor!", "{A} is off to the races!", "{A} leads the break!"}),
                                 carrier->lastName);
        } else {
            setSpots(adv.moves, 1.0f);
            if (RandomFloat() < 0.6f) {
                adv.startText = Fill(PickText({"{A} brings the ball up.", "{A} walks it up the floor.",
                                               "{A} calls the set."}), carrier->lastName);
            }
        }
        placeDefense(adv.moves, carrier->playerId);
        adv.ballFrom = adv.ballTo = carrier->playerId;
        pushBeat(std::move(adv));
    }

    // ----- Half-court ball movement (probing dribbles when no pass is needed), then the drive -----
    const int passBeats = m - static_cast<int>(startIdx);
    const int extras = std::max(0, rem - driveSecs - passBeats);
    std::vector<int> passSecs(static_cast<size_t>(std::max(0, m)), 1);
    int probe = 0;
    if (extras > 0) {
        if (passBeats <= 0) {
            probe = extras;
        } else {
            for (int e = 0; e < extras; ++e) {
                passSecs[startIdx + static_cast<size_t>(RandomInt(0, passBeats - 1))]++;
            }
        }
    }

    // Dribbling and off-ball movement in short bursts, each with fresh spacing
    auto pushSetBeats = [&](const std::shared_ptr<Player>& holder, int seconds) {
        int left = seconds;
        while (left > 0) {
            const int chunk = std::min(left, RandomInt(2, 3));
            PossessionBeat sb = makeBeat("set", chunk, true, 0.6f);
            sb.moves = setMoves(holder->playerId, holder->playerId, true);
            sb.ballFrom = sb.ballTo = holder->playerId;
            if (RandomFloat() < 0.35f) {
                sb.startText = Fill(PickText({"{A} works the dribble.", "{A} probes the defense.", "{A} sizes up his man."}),
                                    holder->lastName);
            }
            pushBeat(std::move(sb));
            left -= chunk;
        }
    };

    if (probe > 0) pushSetBeats(carrier, probe);

    for (size_t k = startIdx; k < static_cast<size_t>(m); ++k) {
        if (passSecs[k] > 1) pushSetBeats(holders[k], passSecs[k] - 1);

        PossessionBeat pb = makeBeat("pass", 1, true, 0.7f);
        pb.moves = setMoves(holders[k]->playerId, holders[k + 1]->playerId, true);
        pb.ballFrom = holders[k]->playerId;
        pb.ballTo = holders[k + 1]->playerId;
        pb.startText = PassText(holders[k], holders[k + 1]);
        if (!screenerId.empty() && RandomFloat() < 0.4f) {
            for (const auto& p : off) {
                if (p->playerId == screenerId) {
                    pb.startText = Fill("{A} uses the screen from {B}.", holders[k]->lastName, p->lastName);
                }
            }
        }
        pushBeat(std::move(pb));
    }

    if (driveSecs > 0) {
        PossessionBeat db = makeBeat("drive", driveSecs, true, 0.95f);
        setSpots(db.moves, 1.0f);
        db.moves[finalId] = rimPoint(5.0f, 8.0f, 5.0f);
        placeDefense(db.moves, finalId);
        db.ballFrom = db.ballTo = finalId;
        db.startText = Fill(PickText({"{A} attacks the rim!", "{A} drives to the basket.", "{A} blows by his man!"}),
                            finalHolder->lastName);
        pushBeat(std::move(db));
    }

    // ----- Final action -----
    const std::string shooterName = finalHolder->lastName;
    const std::string defenderName = plan.defender ? plan.defender->lastName : std::string();
    const bool layup = !plan.isThree && (driveSecs > 0 || plan.fastBreak);

    auto shotStartText = [&]() {
        if (plan.isThree) {
            return Fill(PickText({"{A} rises up for three...", "{A} lets it fly from deep...", "{A} fires from downtown..."}), shooterName);
        }
        if (layup) {
            return Fill(PickText({"{A} goes up strong at the rim...", "{A} lays it up..."}), shooterName);
        }
        return Fill(PickText({"{A} pulls up for the jumper...", "{A} elevates over {B}..."}), shooterName, defenderName);
    };

    // Shooter edges towards the hoop, offensive rebounders crash the glass when the shot is not meant to go in
    auto shotMoves = [&](bool crash) {
        Moves mv;
        const CourtPoint s = posOf(finalId);
        if (!plan.isThree && driveSecs == 0) {
            const float tx = hoop.x - s.x;
            const float ty = hoop.y - s.y;
            const float dist = std::max(0.1f, std::hypot(tx, ty));
            const float step = std::min(plan.fastBreak ? 9.0f : 3.0f, std::max(0.0f, dist - 3.0f));
            mv[finalId] = ClampToCourt(CourtPoint{s.x + tx / dist * step, s.y + ty / dist * step});
        }
        if (crash) {
            std::vector<std::shared_ptr<Player>> crashers;
            for (const auto& p : off) {
                if (p != finalHolder) crashers.push_back(p);
            }
            std::sort(crashers.begin(), crashers.end(), [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
                return a->attributes.physical > b->attributes.physical;
            });
            for (size_t k = 0; k < crashers.size() && k < 2; ++k) {
                mv[crashers[k]->playerId] = rimPoint(5.0f, 9.0f, 8.0f);
                auto guard = guardOf.find(crashers[k]->playerId);
                if (guard != guardOf.end()) mv[guard->second] = rimPoint(3.5f, 7.0f, 8.0f);
            }
        }
        return mv;
    };

    auto reboundMoves = [&](const std::string& reboundId) {
        Moves mv;
        for (const auto& cp : matchState->courtPlayers) {
            mv[cp.playerId] = rimPoint(4.0f, 11.0f, 10.0f);
        }
        mv[reboundId] = rimPoint(2.5f, 4.5f, 4.0f);
        return mv;
    };

    switch (end) {
        case PossessionEnd::MadeShot: {
            PossessionBeat sb = makeBeat("shot", 1, true, 0.9f);
            sb.moves = shotMoves(false);
            sb.ballFrom = sb.ballTo = finalId;
            sb.ballToHoop = true;
            sb.startText = shotStartText();
            std::string made;
            if (plan.isThree) {
                made = Fill(PickText({"{A} hits the three!", "{A} drains it from deep!", "{A} is good from downtown!"}), shooterName);
            } else if (layup) {
                made = Fill(PickText({"{A} finishes at the rim!", "{A} scores inside."}), shooterName);
            } else {
                made = Fill(PickText({"{A} knocks down the jumper!", "{A} hits the mid-range shot."}), shooterName);
            }
            if (assister) made += " (assist " + assister->lastName + ")";
            sb.endText = made;
            sb.onComplete = [this, planCopy, assister]() {
                ApplyScore(planCopy, planCopy.isThree ? 3 : 2);
                if (assister) assister->assistsThisGame++;
                FlipPossession(planCopy);
            };
            pushBeat(std::move(sb));
            break;
        }
        case PossessionEnd::DefensiveRebound:
        case PossessionEnd::OffensiveRebound: {
            const bool offensive = (end == PossessionEnd::OffensiveRebound);
            const std::shared_ptr<Player>& rebounder = offensive ? plan.rebounder : plan.defender;
            const std::string missText = Fill(PickText({"{A} misses.", "{A} can't get it to fall.", "{A} rims out."}), shooterName);
            const std::string boardText = offensive
                ? Fill("{A} fights for the offensive board!", rebounder->lastName)
                : Fill(PickText({"{A} grabs the rebound.", "{A} cleans the glass."}), rebounder->lastName);

            PossessionBeat sb = makeBeat("shot", 1, true, 0.9f);
            sb.moves = shotMoves(true);
            sb.ballFrom = finalId;
            sb.startText = shotStartText();
            if (merged) {
                sb.ballTo = rebounder->playerId;
                sb.endText = missText + " " + boardText;
                sb.onComplete = [this, planCopy, offensive]() { ApplyRebound(planCopy, offensive); };
            } else {
                sb.ballTo = finalId;
                sb.ballToHoop = true;
                sb.endText = missText;
            }
            pushBeat(std::move(sb));

            if (!merged) {
                PossessionBeat rb = makeBeat("rebound", 1, true, 1.0f);
                rb.moves = reboundMoves(rebounder->playerId);
                rb.ballTo = rebounder->playerId;
                rb.endText = boardText;
                rb.onComplete = [this, planCopy, offensive]() { ApplyRebound(planCopy, offensive); };
                pushBeat(std::move(rb));
            }
            break;
        }
        case PossessionEnd::Block: {
            PossessionBeat sb = makeBeat("shot", 1, true, 0.9f);
            sb.moves = shotMoves(false);
            sb.ballFrom = finalId;
            sb.ballTo = plan.defender->playerId;  // swatted straight to the blocker
            sb.startText = shotStartText();
            sb.endText = Fill("{A} blocks {B}!", defenderName, shooterName);
            sb.onComplete = [this, planCopy]() { ApplyBlock(planCopy); };
            pushBeat(std::move(sb));
            break;
        }
        case PossessionEnd::ShootingFoul: {
            PossessionBeat sb = makeBeat("shot", 1, true, 0.9f);
            sb.moves = shotMoves(false);
            sb.ballFrom = sb.ballTo = finalId;
            sb.startText = Fill("{A} attacks the basket...", shooterName);
            sb.endText = Fill("{B} fouls {A}!", shooterName, defenderName);
            sb.onComplete = [this, planCopy]() { ApplyFoul(planCopy); };
            pushBeat(std::move(sb));

            std::vector<bool> results(static_cast<size_t>(plan.ftAttempts), false);
            for (int i = 0; i < plan.ftMade && i < plan.ftAttempts; ++i) results[static_cast<size_t>(i)] = true;
            std::shuffle(results.begin(), results.end(), gen);

            for (int i = 0; i < plan.ftAttempts; ++i) {
                const bool good = results[static_cast<size_t>(i)];
                const bool lastShot = (i == plan.ftAttempts - 1);
                PossessionBeat ft = makeBeat("freethrow", 1, false, 1.0f);
                if (i == 0) {
                    ft.moves[finalId] = CourtPoint{hoopX + dir * 15.0f, kHoopY};
                    int slot = 0;
                    for (const auto& cp : matchState->courtPlayers) {
                        if (cp.playerId == finalId) continue;
                        ft.moves[cp.playerId] = CourtPoint{hoopX + dir * (6.0f + 3.0f * (slot / 2)), (slot % 2 == 0) ? 19.0f : 31.0f};
                        ++slot;
                    }
                }
                ft.ballFrom = ft.ballTo = finalId;
                ft.ballToHoop = true;
                ft.endText = Fill(good ? "{A} makes free throw " : "{A} misses free throw ", shooterName) +
                             std::to_string(i + 1) + " of " + std::to_string(plan.ftAttempts) + ".";
                ft.onComplete = [this, planCopy, good, lastShot]() {
                    if (good) ApplyScore(planCopy, 1);
                    if (lastShot) FlipPossession(planCopy);
                };
                pushBeat(std::move(ft));
            }
            break;
        }
        case PossessionEnd::Steal: {
            PossessionBeat sb = makeBeat("steal", 1, true, 1.0f);
            const CourtPoint a = posOf(finalId);
            const CourtPoint b = posOf(plan.primary->playerId);
            sb.moves[plan.defender->playerId] = ClampToCourt(CourtPoint{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f});
            sb.ballFrom = finalId;
            sb.ballTo = plan.defender->playerId;
            sb.startText = Fill("{A} looks to pass to {B}...", shooterName, plan.primary->lastName);
            sb.endText = Fill("{A} steals it from {B}!", defenderName, shooterName);
            sb.onComplete = [this, planCopy]() { ApplyTurnover(planCopy, true); };
            pushBeat(std::move(sb));
            break;
        }
        case PossessionEnd::Turnover: {
            PossessionBeat sb = makeBeat("turnover", 1, true, 1.0f);
            sb.ballFrom = finalId;
            sb.ballTo = plan.defender->playerId;
            sb.endText = Fill(PickText({"{A} loses the handle, {B} takes it away.", "{A} travels, turnover.",
                                        "{A} throws it away."}), shooterName, defenderName);
            sb.onComplete = [this, planCopy]() { ApplyTurnover(planCopy, false); };
            pushBeat(std::move(sb));
            break;
        }
        case PossessionEnd::None:
            break;
    }

    // Keep the clock honest: the beats that run the clock must add up to the planned possession length
    int clockSum = 0;
    for (const auto& b : beatQueue) {
        if (b.clockRuns) clockSum += b.seconds;
    }
    int diff = plan.duration - clockSum;
    for (auto& b : beatQueue) {
        if (diff == 0) break;
        if (!b.clockRuns) continue;
        if (diff > 0) {
            b.seconds += diff;
            diff = 0;
        } else if (b.seconds + diff >= 1) {
            b.seconds += diff;
            diff = 0;
        } else {
            diff += b.seconds - 1;
            b.seconds = 1;
        }
    }
}

void MatchEngine::StartNextBeat() {
    activeBeat = std::move(beatQueue.front());
    beatQueue.pop_front();
    beatActive = true;
    activeBeatSecondsLeft = std::max(1, activeBeat.seconds);
    activeBeatSecondIndex = 0;
    if (!activeBeat.startText.empty()) EmitFeed(activeBeat.startText);
}

void MatchEngine::PlayBeatSecond() {
    ++activeBeatSecondIndex;
    const bool lastSecond = (activeBeatSecondsLeft == 1);

    // Players walk towards this beat's destinations; the ones that arrived sway a little
    for (auto& cp : matchState->courtPlayers) {
        auto dest = activeBeat.moves.find(cp.playerId);
        if (dest == activeBeat.moves.end()) continue;

        float pace = 10.0f;
        auto actor = actorById.find(cp.playerId);
        if (actor != actorById.end()) pace = static_cast<float>(actor->second->attributes.pace);
        const float maxStep = (8.0f + 0.55f * pace) * activeBeat.speedFactor;

        const float dx = dest->second.x - cp.x;
        const float dy = dest->second.y - cp.y;
        const float dist = std::hypot(dx, dy);
        if (dist <= maxStep) {
            cp.x = dest->second.x + Jit(0.35f);
            cp.y = dest->second.y + Jit(0.35f);
        } else {
            cp.x += dx / dist * maxStep;
            cp.y += dy / dist * maxStep;
        }
        cp.x = Clampf(cp.x, 0.5f, 93.5f);
        cp.y = Clampf(cp.y, 0.5f, 49.5f);
    }

    // The ball stays with its holder, then travels to the next one (or the hoop) in the last second
    BallState& ball = matchState->ball;
    auto findPlayer = [&](const std::string& id, CourtPoint& out) {
        for (const auto& cp : matchState->courtPlayers) {
            if (cp.playerId == id) {
                out = CourtPoint{cp.x, cp.y};
                return true;
            }
        }
        return false;
    };

    CourtPoint where;
    ball.phase = activeBeat.phase;
    if (activeBeat.ballToHoop && lastSecond) {
        ball.x = activeBeat.hoop.x;
        ball.y = activeBeat.hoop.y;
        ball.z = 10.0f;
        ball.inFlight = true;
        ball.holderPlayerId.clear();
    } else if (activeBeat.ballTo != activeBeat.ballFrom && lastSecond) {
        if (findPlayer(activeBeat.ballTo, where)) {
            ball.x = where.x;
            ball.y = where.y;
        }
        ball.z = activeBeat.ballFrom.empty() ? 1.5f : 5.0f;
        ball.inFlight = !activeBeat.ballFrom.empty();
        ball.holderPlayerId.clear();
    } else if (!activeBeat.ballFrom.empty() && findPlayer(activeBeat.ballFrom, where)) {
        ball.x = where.x;
        ball.y = where.y;
        ball.z = 3.5f;
        ball.inFlight = false;
        ball.holderPlayerId = activeBeat.ballFrom;
    }

    for (auto& cp : matchState->courtPlayers) {
        cp.hasBall = (cp.playerId == ball.holderPlayerId);
        cp.action = cp.hasBall ? (activeBeat.phase == "shot" ? "shoot" : (activeBeat.phase == "drive" ? "drive" : "dribble")) : "idle";
    }

    if (activeBeat.clockRuns && matchState->timeRemaining > 0) {
        matchState->timeRemaining--;
        matchState->timeElapsed++;
        ApplyFatigue(1.0f);
    }

    if (--activeBeatSecondsLeft <= 0) CompleteBeat();
}

void MatchEngine::CompleteBeat() {
    if (activeBeat.onComplete) activeBeat.onComplete();
    if (!activeBeat.endText.empty()) EmitFeed(activeBeat.endText);

    // Settle the ball with its new holder
    BallState& ball = matchState->ball;
    if (activeBeat.ballToHoop) {
        ball.x = activeBeat.hoop.x;
        ball.y = activeBeat.hoop.y;
        ball.z = 0.0f;
        ball.inFlight = false;
        ball.holderPlayerId.clear();
    } else if (!activeBeat.ballTo.empty()) {
        for (const auto& cp : matchState->courtPlayers) {
            if (cp.playerId == activeBeat.ballTo) {
                ball.x = cp.x;
                ball.y = cp.y;
                break;
            }
        }
        ball.z = 3.5f;
        ball.inFlight = false;
        ball.holderPlayerId = activeBeat.ballTo;
    }
    for (auto& cp : matchState->courtPlayers) {
        cp.hasBall = (cp.playerId == ball.holderPlayerId);
    }

    beatActive = false;
    if (beatQueue.empty()) FinishPossession();
}

void MatchEngine::FinishPossession() {
    // Dead ball: the right moment for automatic substitutions
    tacticalManager->CheckAutoSubstitutions(matchState->homeTeam, true, matchState->timeElapsed);
    tacticalManager->CheckAutoSubstitutions(matchState->awayTeam, false, matchState->timeElapsed);
    for (const auto& log : tacticalManager->ConsumeSubstitutionLogs()) {
        EmitFeed(log);
    }
}

bool MatchEngine::StepSecond() {
    if (!matchState || matchState->matchComplete) return false;

    // The UI is waiting for the coach: nothing moves until EndBreak()
    if (currentBreak != BreakType::None) return false;
    if (breaksEnabled && !beatActive && beatQueue.empty() && TryStartBreak()) return false;

    if (!beatActive) {
        if (beatQueue.empty()) {
            PossessionPlan plan;
            if (!PlanPossession(plan)) return false;

            if (plan.end == PossessionEnd::None) {
                // No lineup on the floor: let the clock run so the match can still finish
                RebuildCourtRoster();
                PossessionBeat dead;
                dead.phase = "dead";
                dead.seconds = std::max(1, plan.duration);
                beatQueue.push_back(dead);
            } else {
                BuildScript(plan);
            }
        }
        if (beatQueue.empty()) return false;
        StartNextBeat();
    }

    PlayBeatSecond();
    return true;
}

int MatchEngine::StepSeconds(int seconds) {
    int played = 0;
    while (played < seconds && StepSecond()) ++played;
    return played;
}

bool MatchEngine::IsPeriodBreak() const {
    return matchState && !matchState->matchComplete && matchState->timeRemaining <= 0 && !beatActive && beatQueue.empty();
}

// ============================================================================
// Breaks, NBA timeouts and team talks
// ============================================================================

namespace {

// Stable per-player temperament in [0, 1]; salt picks the trait (1 sensitivity, 2 drive, 3 pride)
float TalkTrait(const std::string& id, int salt) {
    const size_t h = std::hash<std::string>{}(id + "#" + std::to_string(salt));
    return static_cast<float>(h % 1000) / 999.0f;
}

const char* TalkVerb(TeamTalkType type) {
    switch (type) {
        case TeamTalkType::Encourage: return "encourages the team";
        case TeamTalkType::Praise: return "praises the team";
        case TeamTalkType::BeAngry: return "loses his temper";
        case TeamTalkType::MoreDefense: return "demands more defense";
        case TeamTalkType::MoreOffense: return "demands more offense";
        case TeamTalkType::StayCalm: return "calls for calm";
        case TeamTalkType::SayNothing: return "says nothing";
    }
    return "talks to the team";
}

} // namespace

void MatchEngine::SetBreaksEnabled(bool enabled) {
    breaksEnabled = enabled;
    breakConsumed = false;
    if (!enabled) {
        currentBreak = BreakType::None;
    } else if (matchState && matchState->possessionCount == 0 && matchState->timeElapsed == 0) {
        StartBreak(BreakType::PreMatch, true);
    }
}

void MatchEngine::StartBreak(BreakType type, bool homeTeam) {
    currentBreak = type;
    breakHome = homeTeam;
    breakTalkGiven = false;
    lastTalk = TeamTalkResult{};

    switch (type) {
        case BreakType::PreMatch:
            EmitFeed(matchState->homeTeam->name + " host " + matchState->awayTeam->name + ".");
            break;
        case BreakType::QuarterBreak:
            EmitFeed("End of quarter " + std::to_string(matchState->quarterNumber) + ".");
            break;
        case BreakType::Halftime:
            EmitFeed("Halftime.");
            break;
        case BreakType::Overtime:
            EmitFeed("Tied after regulation, overtime!");
            break;
        default:
            break;  // GrantTimeout writes its own line
    }
}

void MatchEngine::EndBreak() {
    if (currentBreak == BreakType::None) return;

    if (currentBreak == BreakType::QuarterBreak || currentBreak == BreakType::Halftime ||
        currentBreak == BreakType::Overtime) {
        breakConsumed = true;
    }
    currentBreak = BreakType::None;

    // Substitutions made during the break show on the floor straight away
    if (matchState && !beatActive && beatQueue.empty()) RebuildCourtRoster();
}

bool MatchEngine::CanCallTimeout(bool isHomeTeam) const {
    if (!matchState || matchState->matchComplete) return false;

    const int remaining = isHomeTeam ? homeTimeoutsRemaining : awayTimeoutsRemaining;
    if (remaining <= 0) return false;

    if (matchState->quarterNumber == 4) {
        if ((isHomeTeam ? homeQ4Timeouts : awayQ4Timeouts) >= 4) return false;
        if (matchState->timeRemaining <= 180 && (isHomeTeam ? homeLateTimeouts : awayLateTimeouts) >= 2) return false;
    }
    return true;
}

bool MatchEngine::RequestTimeout(bool isHomeTeam, bool full) {
    if (!matchState || currentBreak != BreakType::None || matchState->timeRemaining <= 0) return false;
    if (!CanCallTimeout(isHomeTeam)) return false;

    int& pending = isHomeTeam ? pendingTimeoutHome : pendingTimeoutAway;
    if (pending != 0) return false;
    pending = full ? 1 : 2;
    return true;
}

bool MatchEngine::GrantTimeout(bool homeTeam) {
    int& pending = homeTeam ? pendingTimeoutHome : pendingTimeoutAway;
    const bool full = (pending == 1);
    pending = 0;
    if (!CanCallTimeout(homeTeam)) return false;

    int& remaining = homeTeam ? homeTimeoutsRemaining : awayTimeoutsRemaining;
    remaining--;
    if (matchState->quarterNumber == 4) {
        (homeTeam ? homeQ4Timeouts : awayQ4Timeouts)++;
        if (matchState->timeRemaining <= 180) (homeTeam ? homeLateTimeouts : awayLateTimeouts)++;
    }

    // A timeout ends the run and lets the legs recover
    homeRun = awayRun = 0;
    const auto& team = homeTeam ? matchState->homeTeam : matchState->awayTeam;
    for (auto& p : team->roster) {
        if (p->onCourt) p->fatigueLevel = std::max(0.0f, p->fatigueLevel - (full ? 0.08f : 0.03f));
    }

    EmitFeed(team->name + (full ? " call a full timeout" : " call a 20-second timeout") +
             " (" + std::to_string(remaining) + " left).");
    StartBreak(full ? BreakType::Timeout : BreakType::ShortTimeout, homeTeam);
    return true;
}

// The away side is run by the computer: it stops the game when the opponent is on a big run
bool MatchEngine::AiWantsTimeout() {
    if (!CanCallTimeout(false) || pendingTimeoutAway != 0) return false;
    if (homeRun < 9) return false;
    if (matchState->timeElapsed - lastAiTimeoutElapsed < 180) return false;
    lastAiTimeoutElapsed = matchState->timeElapsed;
    return true;
}

bool MatchEngine::TryStartBreak() {
    // End of a period: the coach talks to the team (not once the game is decided)
    if (matchState->timeRemaining <= 0 && !breakConsumed) {
        const bool decided = matchState->quarterNumber >= 4 && matchState->homeScore != matchState->awayScore;
        if (decided) return false;

        BreakType type = BreakType::QuarterBreak;
        if (matchState->quarterNumber == 2) type = BreakType::Halftime;
        else if (matchState->quarterNumber >= 4) type = BreakType::Overtime;
        StartBreak(type, true);
        return true;
    }

    if (matchState->timeRemaining > 0) {
        if (pendingTimeoutHome != 0 && GrantTimeout(true)) return true;
        if (pendingTimeoutAway != 0 && GrantTimeout(false)) return true;
        if (AiWantsTimeout()) {
            pendingTimeoutAway = 1;
            if (GrantTimeout(false)) return true;
        }
    }
    return false;
}

bool MatchEngine::ApplyTeamTalk(bool isHomeTeam, TeamTalkType type, int coachMotivation) {
    if (!matchState || currentBreak == BreakType::None || breakTalkGiven) return false;

    const bool shortHuddle = (currentBreak == BreakType::ShortTimeout);
    if (shortHuddle && (type == TeamTalkType::Encourage || type == TeamTalkType::Praise || type == TeamTalkType::BeAngry)) {
        return false;  // twenty seconds only fit a quick instruction
    }

    const bool pre = (currentBreak == BreakType::PreMatch);
    const int margin = isHomeTeam ? (matchState->homeScore - matchState->awayScore)
                                  : (matchState->awayScore - matchState->homeScore);

    // How sensible the message is for the situation, before individual temperament
    float fit = 0.0f;
    switch (type) {
        case TeamTalkType::Encourage:   fit = pre ? 0.5f : (margin < -3 ? 0.65f : 0.25f); break;
        case TeamTalkType::Praise:      fit = pre ? -0.1f : (margin > 3 ? 0.7f : (margin < -3 ? -0.6f : 0.1f)); break;
        case TeamTalkType::BeAngry:     fit = pre ? -0.5f : (margin < -8 ? 0.45f : (margin > 3 ? -0.8f : -0.25f)); break;
        case TeamTalkType::MoreDefense:
        case TeamTalkType::MoreOffense: fit = pre ? 0.3f : (margin < 0 ? 0.45f : 0.25f); break;
        case TeamTalkType::StayCalm:    fit = pre ? 0.35f : (std::abs(margin) <= 8 ? 0.3f : 0.1f); break;
        case TeamTalkType::SayNothing:  fit = (!pre && margin < -6) ? -0.2f : 0.0f; break;
    }

    const float strength = shortHuddle ? 0.6f : 1.0f;
    fit *= 0.7f;  // even a perfect message never convinces everybody
    const float motivationBonus = (std::clamp(static_cast<float>(coachMotivation), 40.0f, 99.0f) - 70.0f) / 100.0f * 0.4f;
    const auto& team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;

    TeamTalkResult result;
    result.valid = true;
    result.isHome = isHomeTeam;
    result.type = type;

    int positive = 0;
    int negative = 0;
    for (const auto& p : team->roster) {
        if (!p || !p->active) continue;

        const float sensitivity = 0.6f + 0.8f * TalkTrait(p->playerId, 1);
        const float drive = TalkTrait(p->playerId, 2);
        const float pride = TalkTrait(p->playerId, 3);

        float r = fit * sensitivity;
        if (type == TeamTalkType::BeAngry) r += (drive - 0.5f) * 0.5f - (pride - 0.5f) * 0.7f;
        if (type == TeamTalkType::MoreDefense || type == TeamTalkType::MoreOffense) r += (drive - 0.5f) * 0.25f;
        if (type != TeamTalkType::SayNothing) r += motivationBonus + RandomFloatRange(-0.45f, 0.45f);

        const int mood = r > 0.22f ? 1 : (r < -0.18f ? -1 : 0);
        float delta = (mood != 0) ? std::clamp(r * 0.30f, -0.30f, 0.30f) : r * 0.05f;
        delta *= strength;
        p->morale = std::clamp(p->morale + delta, -1.0f, 1.0f);

        if (mood > 0) positive++;
        if (mood < 0) negative++;

        TalkReaction reaction;
        reaction.playerId = p->playerId;
        reaction.name = p->firstName + " " + p->lastName;
        reaction.mood = mood;
        reaction.delta = delta;
        result.reactions.push_back(reaction);
    }

    // Players on the floor first, then by how strongly they reacted
    std::stable_sort(result.reactions.begin(), result.reactions.end(), [&](const TalkReaction& a, const TalkReaction& b) {
        bool aOn = false, bOn = false;
        for (const auto& p : team->roster) {
            if (p->playerId == a.playerId) aOn = p->onCourt;
            if (p->playerId == b.playerId) bOn = p->onCourt;
        }
        return aOn != bOn ? aOn : false;
    });

    const int total = std::max(1, positive + negative + static_cast<int>(std::count_if(
        result.reactions.begin(), result.reactions.end(), [](const TalkReaction& r) { return r.mood == 0; })));
    if (positive * 10 >= total * 7) result.summary = "The whole team responds well.";
    else if (negative * 10 >= total * 4) result.summary = "The dressing room does not like the tone.";
    else if (positive > negative) result.summary = "Most players respond well.";
    else if (negative > positive) result.summary = "Some players take it badly.";
    else result.summary = "The players are unmoved.";

    // Tactical side effects
    if (type == TeamTalkType::MoreDefense) ApplyCoachingCallout(isHomeTeam, CoachingCallout::MORE_DEFENSIVE);
    if (type == TeamTalkType::MoreOffense) ApplyCoachingCallout(isHomeTeam, CoachingCallout::MORE_OFFENSIVE);
    if (type == TeamTalkType::StayCalm) {
        for (auto& p : team->roster) {
            if (p->onCourt) p->fatigueLevel = std::max(0.0f, p->fatigueLevel - 0.03f);
        }
    }

    breakTalkGiven = true;
    lastTalk = result;
    EmitFeed(std::string("The coach ") + TalkVerb(type) + ". " + result.summary);
    return true;
}

// ============================================================================
// Offense schemes, playbook, individual feedback and queued substitutions
// ============================================================================

namespace {

const char* SchemeName(OffensiveStrategy scheme) {
    switch (scheme) {
        case OffensiveStrategy::THREE_POINT_HEAVY: return "shoot more threes";
        case OffensiveStrategy::INSIDE_FOCUSED: return "play more inside";
        case OffensiveStrategy::PICK_AND_ROLL: return "pick and roll";
        case OffensiveStrategy::BALL_MOVEMENT: return "move the ball around";
        case OffensiveStrategy::PLAY_THROUGH_PLAYER: return "play through a player";
        case OffensiveStrategy::PACE_AND_SPACE: return "pace and space";
        case OffensiveStrategy::ISOLATION: return "isolation";
        case OffensiveStrategy::FAST_BREAK: return "push the pace";
        default: return "balanced offense";
    }
}

const char* InstructionLabel(PlayerInstruction instruction) {
    switch (instruction) {
        case PlayerInstruction::SettleDown: return "settle down";
        case PlayerInstruction::FocusOnDefense: return "focus on defense";
        case PlayerInstruction::LookForYourShot: return "look for your shot";
        default: return "";
    }
}

} // namespace

const std::vector<Play>& MatchEngine::GetPlaybook() {
    static const std::vector<Play> playbook = {
        {"five_out", "Five Out", OffensiveStrategy::THREE_POINT_HEAVY, +0.10f, +0.010f, 0.000f},
        {"transition_threes", "Transition Threes", OffensiveStrategy::THREE_POINT_HEAVY, +0.15f, -0.010f, +0.010f},
        {"post_split", "Post Split", OffensiveStrategy::INSIDE_FOCUSED, -0.08f, +0.020f, 0.000f},
        {"dribble_drive", "Dribble Drive", OffensiveStrategy::INSIDE_FOCUSED, -0.06f, +0.010f, +0.010f},
        {"high_pick_and_roll", "High Pick and Roll", OffensiveStrategy::PICK_AND_ROLL, 0.000f, +0.030f, 0.000f},
        {"spain_pick_and_roll", "Spain Pick and Roll", OffensiveStrategy::PICK_AND_ROLL, +0.050f, +0.035f, +0.005f},
        {"motion", "Motion Offense", OffensiveStrategy::BALL_MOVEMENT, +0.050f, +0.030f, +0.010f},
        {"flow", "Flow Offense", OffensiveStrategy::BALL_MOVEMENT, 0.000f, +0.025f, 0.000f},
        {"iso_star", "Star Isolation", OffensiveStrategy::PLAY_THROUGH_PLAYER, 0.000f, 0.000f, 0.000f},
        {"elbow_hub", "Elbow Hub", OffensiveStrategy::PLAY_THROUGH_PLAYER, -0.050f, +0.010f, 0.000f},
    };
    return playbook;
}

bool MatchEngine::SetOffenseScheme(bool isHomeTeam, OffensiveStrategy scheme, const std::string& targetPlayerId,
                                   const std::string& playId, std::string* error) {
    auto fail = [&](const char* message) {
        if (error) *error = message;
        return false;
    };
    if (!matchState) return fail("No match is running");

    const auto& team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;
    std::string hubName;
    if (scheme == OffensiveStrategy::PLAY_THROUGH_PLAYER) {
        if (targetPlayerId.empty()) return fail("Choose the player to play through");
        bool onFloor = false;
        for (const auto& p : team->roster) {
            if (p->playerId == targetPlayerId && p->onCourt) {
                onFloor = true;
                hubName = p->lastName;
            }
        }
        if (!onFloor) return fail("The chosen player must be on the floor");
    }

    const Play* play = nullptr;
    if (!playId.empty()) {
        for (const auto& candidate : GetPlaybook()) {
            if (candidate.id == playId) play = &candidate;
        }
        if (!play) return fail("Unknown play");
        if (play->scheme != scheme) return fail("That play does not belong to the chosen scheme");
    }

    if (isHomeTeam) tacticalManager->SetHomeOffensiveStrategy(scheme);
    else tacticalManager->SetAwayOffensiveStrategy(scheme);

    const int side = isHomeTeam ? 0 : 1;
    offenseTarget[side] = (scheme == OffensiveStrategy::PLAY_THROUGH_PLAYER) ? targetPlayerId : std::string();
    offensePlay[side] = playId;

    std::string line = team->name + ": " + SchemeName(scheme);
    if (!hubName.empty()) line += " (" + hubName + ")";
    if (play) line += ", " + play->name;
    EmitFeed(line + ".");
    return true;
}

float MatchEngine::InstructionStrength(const std::string& playerId, PlayerInstruction type) const {
    auto it = instructions.find(playerId);
    return (it != instructions.end() && it->second.type == type) ? it->second.strength : 0.0f;
}

PlayerInstruction MatchEngine::GetInstruction(const std::string& playerId) const {
    auto it = instructions.find(playerId);
    return it != instructions.end() ? it->second.type : PlayerInstruction::None;
}

bool MatchEngine::GiveFeedback(bool isHomeTeam, const std::string& playerId, PlayerInstruction instruction,
                               int coachMotivation) {
    if (!matchState || instruction == PlayerInstruction::None) return false;

    const auto& team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;
    std::shared_ptr<Player> target;
    for (const auto& p : team->roster) {
        if (p->playerId == playerId && p->onCourt) target = p;
    }
    if (!target) return false;  // feedback goes to a player on the floor

    const float sensitivity = 0.6f + 0.8f * TalkTrait(playerId, 1);
    const float drive = TalkTrait(playerId, 2);
    const float pride = TalkTrait(playerId, 3);
    const float motivationBonus = (std::clamp(static_cast<float>(coachMotivation), 40.0f, 99.0f) - 70.0f) / 100.0f * 0.4f;

    float r = 0.25f * sensitivity + motivationBonus + RandomFloatRange(-0.35f, 0.35f);
    switch (instruction) {
        case PlayerInstruction::SettleDown:
            r += 0.10f - (pride - 0.5f) * 0.6f;
            break;
        case PlayerInstruction::FocusOnDefense:
            r += (drive - 0.5f) * 0.4f - (target->attributes.shooting >= 15 ? 0.15f : 0.0f);  // scorers dislike fewer shots
            break;
        case PlayerInstruction::LookForYourShot:
            r += (drive - 0.5f) * 0.3f + (target->attributes.shooting - 10) / 40.0f;
            break;
        case PlayerInstruction::None:
            break;
    }

    const int mood = r > 0.20f ? 1 : (r < -0.10f ? -1 : 0);
    ActiveInstruction active;
    active.type = instruction;
    active.strength = mood > 0 ? 1.0f : (mood == 0 ? 0.7f : 0.4f);
    active.possessionsLeft = 14;
    instructions[playerId] = active;
    target->morale = std::clamp(target->morale + 0.08f * static_cast<float>(mood), -1.0f, 1.0f);

    lastFeedback = PlayerFeedbackResult{};
    lastFeedback.valid = true;
    lastFeedback.playerId = playerId;
    lastFeedback.name = target->firstName + " " + target->lastName;
    lastFeedback.instruction = instruction;
    lastFeedback.mood = mood;
    lastFeedback.text = std::string(mood > 0 ? "responds well" : (mood < 0 ? "takes it badly, half-hearted" : "listens, but is not fully convinced"));

    EmitFeed("Coach to " + target->lastName + ": " + InstructionLabel(instruction) + ". He " + lastFeedback.text + ".");
    return true;
}

bool MatchEngine::RequestSubstitution(bool isHomeTeam, const std::string& outPlayerId, const std::string& inPlayerId) {
    if (!matchState || matchState->matchComplete || outPlayerId == inPlayerId) return false;

    const auto& team = isHomeTeam ? matchState->homeTeam : matchState->awayTeam;
    std::shared_ptr<Player> out;
    std::shared_ptr<Player> in;
    for (const auto& p : team->roster) {
        if (p->playerId == outPlayerId) out = p;
        if (p->playerId == inPlayerId) in = p;
    }
    if (!out || !in || !out->onCourt || in->onCourt || in->injured || !in->active) return false;

    for (const auto& queued : pendingSubs) {
        if (queued.outId == outPlayerId || queued.inId == inPlayerId || queued.outId == inPlayerId) return false;
    }
    pendingSubs.push_back(PendingSub{isHomeTeam, outPlayerId, inPlayerId});

    // During a break or a dead ball the change happens now, otherwise at the next dead ball
    if (IsBreakActive() || (!beatActive && beatQueue.empty())) {
        ApplyPendingSubstitutions();
        if (!beatActive && beatQueue.empty() && !matchState->courtPlayers.empty()) RebuildCourtRoster();
    }
    return true;
}

void MatchEngine::ApplyPendingSubstitutions() {
    if (pendingSubs.empty() || !matchState) return;

    for (const auto& sub : pendingSubs) {
        const auto& team = sub.home ? matchState->homeTeam : matchState->awayTeam;
        std::shared_ptr<Player> out;
        std::shared_ptr<Player> in;
        for (const auto& p : team->roster) {
            if (p->playerId == sub.outId) out = p;
            if (p->playerId == sub.inId) in = p;
        }
        if (!out || !in || !tacticalManager->ManualSubstitution(team, out, in, matchState->timeElapsed)) continue;
        instructions.erase(out->playerId);
        EmitFeed("Substitution: " + in->lastName + " in for " + out->lastName + ".");

        // The coach took the featured player off: the plan falls back to moving the ball
        const int side = sub.home ? 0 : 1;
        if (offenseTarget[side] == out->playerId) {
            if (sub.home) tacticalManager->SetHomeOffensiveStrategy(OffensiveStrategy::BALL_MOVEMENT);
            else tacticalManager->SetAwayOffensiveStrategy(OffensiveStrategy::BALL_MOVEMENT);
            offenseTarget[side].clear();
            offensePlay[side].clear();
            EmitFeed(team->name + ": without " + out->lastName + " the team moves the ball instead.");
        }
    }
    pendingSubs.clear();
}

} // namespace engine
} // namespace bm