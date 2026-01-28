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
                             homeTimeoutsRemaining(3), awayTimeoutsRemaining(3) {
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

    float roll = RandomFloat();
    float pointsScored = 0;

    if (roll < stealChance) {
        // Steal/turnover
        oss << passer->firstName << " swings to " << ballHandler->lastName << ", stolen by "
            << defender->lastName << "!";
        matchState->homeTeamPossession = !matchState->homeTeamPossession;
    } else if (roll < stealChance + foulChance) {
        // Shooting foul leads to free throws
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
        oss << ballHandler->firstName << " " << ballHandler->lastName << " loses the handle (turnover).";
        matchState->homeTeamPossession = !matchState->homeTeamPossession;
    } else {
        // Shot attempt
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
            oss << defender->lastName << " rejects " << ballHandler->lastName << "!";
            defender->reboundsThisGame++;
            matchState->lastRebounder = defender;
            if (matchState->homeTeamPossession) {
                matchState->awayRebounds++;
            } else {
                matchState->homeRebounds++;
            }
            matchState->homeTeamPossession = !matchState->homeTeamPossession;
        } else if (RandomFloat() < shotMakeProb) {
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
        reserve->playerId = team->name + "_reserve_" + std::to_string(i);
        reserve->firstName = "Reserve";
        reserve->lastName = std::to_string(i + 1);
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
        std::cout << "║  6. Quit Game                                                  ║\n";
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
                
            case 6:
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

} // namespace engine
} // namespace bm