#include "engine/MatchEngine.h"
#include "engine/TacticalManager.h"
#include "common/Types.h"
#include <chrono>
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>
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
                             homeTimeoutsRemaining(3), awayTimeoutsRemaining(3),
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
    
    // Reset timeouts (3 per half)
    homeTimeoutsRemaining = 3;
    awayTimeoutsRemaining = 3;
    
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

    std::this_thread::sleep_for(std::chrono::milliseconds(800 / speedMultiplier));
}

int MatchEngine::SimulatePossession() {
    if (!matchState) return 0;

    matchState->possessionCount++;

    // Variable possession duration to drive pace (~12-20 seconds)
    int durationSeconds = std::max(6, static_cast<int>(std::round(RandomFloatRange(12.0f, 20.0f))));
    durationSeconds = std::min(durationSeconds, std::max(1, matchState->timeRemaining));

    // Determine which team has ball
    auto& offensiveTeam = matchState->homeTeamPossession ? matchState->homeTeam : matchState->awayTeam;
    auto& defensiveTeam = matchState->homeTeamPossession ? matchState->awayTeam : matchState->homeTeam;

    // Get players on court
    std::vector<std::shared_ptr<Player>> onCourtOffense, onCourtDefense;
    for (auto& p : offensiveTeam->roster) {
        if (p->onCourt) onCourtOffense.push_back(p);
    }
    for (auto& p : defensiveTeam->roster) {
        if (p->onCourt) onCourtDefense.push_back(p);
    }

    if (onCourtOffense.empty() || onCourtDefense.empty()) {
        return durationSeconds;
    }

    // Pick random players
    auto ballHandler = onCourtOffense[RandomInt(0, static_cast<int>(onCourtOffense.size()) - 1)];
    auto defender = onCourtDefense[RandomInt(0, static_cast<int>(onCourtDefense.size()) - 1)];
    auto passer = onCourtOffense[RandomInt(0, static_cast<int>(onCourtOffense.size()) - 1)];
    if (passer == ballHandler && onCourtOffense.size() > 1) {
        passer = onCourtOffense[(RandomInt(1, static_cast<int>(onCourtOffense.size()) - 1)) % onCourtOffense.size()];
    }

    // Tactical context
    auto offenseStrategy = matchState->homeTeamPossession ? tacticalManager->GetHomeOffensiveStrategy()
                                                          : tacticalManager->GetAwayOffensiveStrategy();
    auto defenseStrategy = matchState->homeTeamPossession ? tacticalManager->GetAwayDefensiveStrategy()
                                                          : tacticalManager->GetHomeDefensiveStrategy();

    float defensiveIntensity = tacticalManager->GetDefensiveIntensity(defenseStrategy);
    float offensiveRating = CalculatePlaySuccessRate(*ballHandler);
    float defensiveRating = CalculateDefenseEffectiveness(onCourtDefense, defensiveIntensity);

    // Fatigue impacts
    offensiveRating *= (1.0f - (ballHandler->fatigueLevel * 0.25f));
    defensiveRating *= (1.0f - (defender->fatigueLevel * 0.15f));

    // Base chances
    float stealChance = 0.08f + std::max(0.0f, defensiveIntensity - 1.0f) * 0.04f;
    float foulChance = 0.12f + std::max(0.0f, defensiveIntensity - 1.0f) * 0.03f;
    float turnoverChance = 0.06f;

    // Shot selection probabilities
    float threePointChance = tacticalManager->GetThreePointPercentage(offenseStrategy);
    if (ballHandler->position == Position::PG || ballHandler->position == Position::SG) {
        threePointChance += 0.05f;
    }
    threePointChance = std::min(0.8f, std::max(0.1f, threePointChance));

    // Build log
    std::ostringstream oss;
    std::string actionPhase = "dribble";

    float roll = RandomFloat();
    float pointsScored = 0;

    if (roll < stealChance) {
        // Steal/turnover
        actionPhase = "steal";
        oss << passer->firstName << " swings to " << ballHandler->lastName << ", stolen by "
            << defender->lastName << "!";
        defender->stealsThisGame++;
        ballHandler->turnoversThisGame++;
        if (matchState->homeTeamPossession) {
            matchState->homeTurnovers++;
            matchState->awaySteals++;
        } else {
            matchState->awayTurnovers++;
            matchState->homeSteals++;
        }
        matchState->homeTeamPossession = !matchState->homeTeamPossession;
    } else if (roll < stealChance + foulChance) {
        // Shooting foul leads to free throws
        actionPhase = "foul";
        float ftPct = std::clamp(0.72f + (ballHandler->attributes.shooting / 20.0f) * 0.1f, 0.6f, 0.9f);
        int attempts = 2;
        // Occasional and-1 or three-shot foul
        if (RandomFloat() < 0.1f) attempts = 3;
        int made = 0;
        for (int i = 0; i < attempts; ++i) {
            if (RandomFloat() < ftPct) made++;
        }
        pointsScored = static_cast<float>(made);
        oss << ballHandler->firstName << " " << ballHandler->lastName << " fouled by "
            << defender->lastName << " - hits " << made << "/" << attempts << " at the line.";
        if (matchState->homeTeamPossession) {
            matchState->awayFouls++;
            matchState->homeScore += pointsScored;
        } else {
            matchState->homeFouls++;
            matchState->awayScore += pointsScored;
        }
        defender->foulsThisGame++;
        ballHandler->pointsThisGame += pointsScored;
        matchState->lastScorer = ballHandler;
        matchState->homeTeamPossession = !matchState->homeTeamPossession;
    } else if (roll < stealChance + foulChance + turnoverChance) {
        // Unforced turnover
        actionPhase = "turnover";
        oss << ballHandler->firstName << " " << ballHandler->lastName << " loses the handle (turnover).";
        ballHandler->turnoversThisGame++;
        if (matchState->homeTeamPossession) {
            matchState->homeTurnovers++;
        } else {
            matchState->awayTurnovers++;
        }
        matchState->homeTeamPossession = !matchState->homeTeamPossession;
    } else {
        // Shot attempt
        actionPhase = "shot";
        bool isThree = (RandomFloat() < threePointChance);
        float base2pt = 0.44f + (ballHandler->attributes.shooting / 20.0f) * 0.14f;
        base2pt *= (1.0f + (ballHandler->attributes.technical / 20.0f) * 0.05f);
        float fg2 = std::clamp(base2pt * (offensiveRating / 0.5f) * (1.0f - defensiveRating * 0.15f)
                               * (1.0f - ballHandler->fatigueLevel * 0.2f), 0.35f, 0.7f);
        float fg3 = std::clamp(fg2 - 0.08f, 0.26f, 0.55f);
        float shotMakeProb = isThree ? fg3 : fg2;

        float blockChance = std::clamp(0.04f + defensiveIntensity * 0.03f, 0.02f, 0.12f);

        if (RandomFloat() < blockChance) {
            // Blocked shot -> defensive rebound
            actionPhase = "block";
            oss << defender->lastName << " rejects " << ballHandler->lastName << "!";
            defender->blocksThisGame++;
            defender->reboundsThisGame++;
            matchState->lastRebounder = defender;
            if (matchState->homeTeamPossession) {
                matchState->awayBlocks++;
                matchState->awayRebounds++;
            } else {
                matchState->homeBlocks++;
                matchState->homeRebounds++;
            }
            matchState->homeTeamPossession = !matchState->homeTeamPossession;
        } else if (RandomFloat() < shotMakeProb) {
            actionPhase = isThree ? "three_make" : "shot_make";
            pointsScored = isThree ? 3.0f : 2.0f;
            oss << passer->firstName << " finds " << ballHandler->lastName << " for "
                << (isThree ? "a 3!" : "two!");
            if (matchState->homeTeamPossession) {
                matchState->homeScore += pointsScored;
            } else {
                matchState->awayScore += pointsScored;
            }
            ballHandler->pointsThisGame += pointsScored;
            matchState->lastScorer = ballHandler;
            matchState->homeTeamPossession = !matchState->homeTeamPossession;
        } else {
            // Missed shot
            actionPhase = "miss";
            float offensiveRebChance = std::clamp(0.27f - (defensiveIntensity - 1.0f) * 0.06f, 0.18f, 0.32f);
            bool offensiveRebound = (RandomFloat() < offensiveRebChance);

            if (offensiveRebound) {
                auto rebounder = onCourtOffense[RandomInt(0, static_cast<int>(onCourtOffense.size()) - 1)];
                rebounder->reboundsThisGame++;
                matchState->lastRebounder = rebounder;
                if (matchState->homeTeamPossession) {
                    matchState->homeRebounds++;
                } else {
                    matchState->awayRebounds++;
                }
                oss << ballHandler->lastName << " misses, " << rebounder->lastName << " with the offensive board.";
            } else {
                defender->reboundsThisGame++;
                matchState->lastRebounder = defender;
                if (matchState->homeTeamPossession) {
                    matchState->awayRebounds++;
                } else {
                    matchState->homeRebounds++;
                }
                oss << ballHandler->lastName << " misses, rebound " << defender->lastName << ".";
                matchState->homeTeamPossession = !matchState->homeTeamPossession;
            }
        }
    }

    matchState->lastPlay = oss.str();

    // Update spatial player/ball coordinates for 2D/3D visualization
    UpdateSpatialState(ballHandler, defender, actionPhase);

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

    // Optional pacing delay based on speed multiplier
    int baseDelayMs = 400;
    if (speedMultiplier > 0) {
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

void MatchEngine::SimulateQuarter(int quarterNumber) {
    if (!matchState) return;
    
    matchState->quarterNumber = quarterNumber;
    matchState->timeRemaining = 720;  // Reset to 12 minutes per quarter
    
    // Reset timeouts at halftime
    if (quarterNumber == 3) {
        homeTimeoutsRemaining = 3;
        awayTimeoutsRemaining = 3;
        std::cout << "\n[HALFTIME] Timeouts reset to 3 per team\n";
    }
    
    std::cout << "\n═════════════════ QUARTER " << quarterNumber << " ═════════════════\n";

    // Possession arrow rules:
    // Q1: Jump ball determines winner
    // Q2 & Q3: Loser of Q1 tip gets possession  
    // Q4: Winner of Q1 tip gets possession back
    // OT: Jump ball again
    if (quarterNumber == 1) {
        SimulateJumpBall();
    } else if (quarterNumber >= 5) {
        // Overtime - jump ball again
        SimulateJumpBall();
    } else {
        // Q2, Q3, Q4 - use possession arrow
        if (quarterNumber == 2 || quarterNumber == 3) {
            // Loser of Q1 tip starts Q2 and Q3
            matchState->homeTeamPossession = !matchState->jumpBallWinner;
            std::string startingTeam = matchState->homeTeamPossession ? 
                matchState->homeTeam->name : matchState->awayTeam->name;
            std::cout << "\n[POSSESSION ARROW] " << startingTeam << " starts with the ball\n";
        } else if (quarterNumber == 4) {
            // Winner of Q1 tip gets it back for Q4
            matchState->homeTeamPossession = matchState->jumpBallWinner;
            std::string startingTeam = matchState->homeTeamPossession ? 
                matchState->homeTeam->name : matchState->awayTeam->name;
            std::cout << "\n[POSSESSION ARROW] " << startingTeam << " starts with the ball\n";
        }
    }

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
    std::this_thread::sleep_for(std::chrono::milliseconds(500 / speedMultiplier));
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
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / speedMultiplier));
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

} // namespace engine
} // namespace bm