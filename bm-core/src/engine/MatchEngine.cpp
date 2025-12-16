#include "engine/MatchEngine.h"
#include "common/Types.h"
#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>

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

// Helper function to get random int [min, max]
static int RandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

// Possession outcome enum
enum class PossessionOutcome {
    TWO_POINTER,
    THREE_POINTER,
    MISS_OFFENSIVE_REBOUND,
    MISS_DEFENSIVE_REBOUND,
    TURNOVER,
    FOUL
};

// Current possession data
struct CurrentPossession {
    bool homeTeamBall;
    std::shared_ptr<Player> ballHandler;
    std::shared_ptr<Player> defender;
    float offensiveRating;
    float defensiveRating;
    PossessionOutcome outcome;
    int pointsScored;
    std::string playDescription;
};

MatchEngine::MatchEngine() : matchState(nullptr) {
    std::cout << "[MatchEngine] Initialized\n";
}

MatchEngine::~MatchEngine() {
}

void MatchEngine::InitializeMatch(std::shared_ptr<Team> homeTeam, std::shared_ptr<Team> awayTeam) {
    std::cout << "[MatchEngine] Initializing match: " << homeTeam->name << " vs " << awayTeam->name << "\n";
    
    // Create new match state
    matchState = std::make_shared<MatchState>();
    matchState->homeTeam = homeTeam;
    matchState->awayTeam = awayTeam;
    matchState->homeScore = 0;
    matchState->awayScore = 0;
    matchState->quarterNumber = 1;
    matchState->timeRemaining = 600; // 10 minutes per quarter
    matchState->homeRebounds = 0;
    matchState->awayRebounds = 0;
    matchState->homeFouls = 0;
    matchState->awayFouls = 0;
    matchState->matchComplete = false;
    
    std::cout << "[MatchEngine] Match initialized - " << homeTeam->name << " 0-0 " << awayTeam->name << "\n";
}

void MatchEngine::SimulatePossession() {
    if (!matchState) return;
    
    // Determine which team has ball (alternate)
    static bool homeHasBall = true;
    
    CurrentPossession possession;
    possession.homeTeamBall = homeHasBall;
    
    // Select ball handler and defender
    auto& offensiveTeam = homeHasBall ? matchState->homeTeam : matchState->awayTeam;
    auto& defensiveTeam = homeHasBall ? matchState->awayTeam : matchState->homeTeam;
    
    if (offensiveTeam->roster.empty() || defensiveTeam->roster.empty()) {
        std::cout << "[MatchEngine] ERROR: Empty roster!\n";
        return;
    }
    
    // Pick random player from offense
    possession.ballHandler = offensiveTeam->roster[RandomInt(0, offensiveTeam->roster.size() - 1)];
    possession.defender = defensiveTeam->roster[RandomInt(0, defensiveTeam->roster.size() - 1)];
    
    // 1. Play selection
    PlaySelection();
    
    // 2. Calculate offensive and defensive ratings
    possession.offensiveRating = CalculatePlaySuccessRate(*possession.ballHandler);
    possession.defensiveRating = CalculateDefenseEffectiveness();
    
    // 3. Determine outcome
    float successChance = possession.offensiveRating - possession.defensiveRating;
    successChance = std::max(0.2f, std::min(0.8f, successChance)); // Clamp between 20-80%
    
    float roll = RandomFloat();
    
    if (roll < 0.08f) {
        // Turnover (8% - NCAA average is ~13 turnovers per team)
        possession.outcome = PossessionOutcome::TURNOVER;
        possession.pointsScored = 0;
        possession.playDescription = possession.ballHandler->firstName + " " + possession.ballHandler->lastName + 
                                    " loses the ball! Turnover!";
        homeHasBall = !homeHasBall; // Change possession
    }
    else if (roll < 0.14f) {
        // Foul (6% - NCAA average is ~20 fouls per team)
        possession.outcome = PossessionOutcome::FOUL;
        possession.pointsScored = RandomInt(0, 2); // 0-2 free throws made
        possession.playDescription = "Foul on " + possession.defender->firstName + " " + 
                                    possession.defender->lastName + "! " + 
                                    std::to_string(possession.pointsScored) + " free throws made.";
        
        if (homeHasBall) {
            matchState->awayFouls++;
            matchState->homeScore += possession.pointsScored;
        } else {
            matchState->homeFouls++;
            matchState->awayScore += possession.pointsScored;
        }
        homeHasBall = !homeHasBall;
    }
    else if (roll < successChance) {
        // Shot attempt - success
        bool isThree = (roll < successChance * 0.35f); // 35% of shots are 3-pointers
        
        if (isThree) {
            possession.outcome = PossessionOutcome::THREE_POINTER;
            possession.pointsScored = 3;
            possession.playDescription = possession.ballHandler->firstName + " " + 
                                        possession.ballHandler->lastName + 
                                        " for THREE! It's good!";
        } else {
            possession.outcome = PossessionOutcome::TWO_POINTER;
            possession.pointsScored = 2;
            possession.playDescription = possession.ballHandler->firstName + " " + 
                                        possession.ballHandler->lastName + 
                                        " drives... scores!";
        }
        
        if (homeHasBall) {
            matchState->homeScore += possession.pointsScored;
        } else {
            matchState->awayScore += possession.pointsScored;
        }
        
        matchState->lastScorer = possession.ballHandler;
        homeHasBall = !homeHasBall; // Change possession after score
    }
    else {
        // Shot attempt - miss
        bool offensiveRebound = (RandomFloat() < 0.30f); // 30% chance offensive rebound
        
        if (offensiveRebound) {
            possession.outcome = PossessionOutcome::MISS_OFFENSIVE_REBOUND;
            possession.pointsScored = 0;
            possession.playDescription = possession.ballHandler->firstName + " " + 
                                        possession.ballHandler->lastName + 
                                        " misses! Offensive rebound, they keep possession!";
            if (homeHasBall) matchState->homeRebounds++;
            else matchState->awayRebounds++;
            // Keep same possession
        } else {
            possession.outcome = PossessionOutcome::MISS_DEFENSIVE_REBOUND;
            possession.pointsScored = 0;
            possession.playDescription = possession.ballHandler->firstName + " " + 
                                        possession.ballHandler->lastName + 
                                        " misses! " + possession.defender->firstName + " " + 
                                        possession.defender->lastName + " grabs the rebound!";
            if (homeHasBall) matchState->awayRebounds++;
            else matchState->homeRebounds++;
            homeHasBall = !homeHasBall; // Change possession
        }
    }
    
    matchState->lastPlay = possession.playDescription;
    
    // 4. Apply fatigue
    ApplyFatigue();
    
    // Output commentary
    std::cout << possession.playDescription << " [" 
              << matchState->homeTeam->name << " " << matchState->homeScore 
              << " - " << matchState->awayScore << " " 
              << matchState->awayTeam->name << "]\n";
}

void MatchEngine::SimulateQuarter(int quarterNumber) {
    if (!matchState) return;
    
    matchState->quarterNumber = quarterNumber;
    std::cout << "\n========== QUARTER " << quarterNumber << " ==========\n";
    
    // Simulate ~45 possessions per quarter (90 per team, 180 total)
    // NCAA teams average ~70 possessions per game
    // With 50% FG% and some 3-pointers, this gives realistic 65-85 point games
    for (int i = 0; i < 45; ++i) {
        SimulatePossession();
    }
    
    std::cout << "========== END Q" << quarterNumber << " ==========\n";
    std::cout << matchState->homeTeam->name << " " << matchState->homeScore 
              << " - " << matchState->awayScore << " " 
              << matchState->awayTeam->name << "\n\n";
}

void MatchEngine::SimulateFullMatch() {
    if (!matchState) {
        std::cout << "[MatchEngine] ERROR: Match not initialized!\n";
        return;
    }
    
    std::cout << "\n╔════════════════════════════════════════════════╗\n";
    std::cout << "║          NCAA BASKETBALL MATCH                 ║\n";
    std::cout << "╠════════════════════════════════════════════════╣\n";
    std::cout << "║  " << matchState->homeTeam->name;
    for(int i = matchState->homeTeam->name.length(); i < 20; i++) std::cout << " ";
    std::cout << " vs " << matchState->awayTeam->name;
    for(int i = matchState->awayTeam->name.length(); i < 20; i++) std::cout << " ";
    std::cout << " ║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n\n";
    
    // Simulate 4 quarters
    for (int q = 1; q <= 4; ++q) {
        SimulateQuarter(q);
    }
    
    // Check for overtime
    if (matchState->homeScore == matchState->awayScore) {
        std::cout << "\n========== OVERTIME ==========\n";
        for (int i = 0; i < 12; ++i) {
            SimulatePossession();
        }
    }
    
    matchState->matchComplete = true;
    
    // Final score
    std::cout << "\n╔════════════════════════════════════════════════╗\n";
    std::cout << "║                 FINAL SCORE                    ║\n";
    std::cout << "╠════════════════════════════════════════════════╣\n";
    std::cout << "║  " << matchState->homeTeam->name << ": " << matchState->homeScore;
    for(int i = matchState->homeTeam->name.length() + std::to_string(matchState->homeScore).length(); i < 43; i++) 
        std::cout << " ";
    std::cout << "║\n";
    std::cout << "║  " << matchState->awayTeam->name << ": " << matchState->awayScore;
    for(int i = matchState->awayTeam->name.length() + std::to_string(matchState->awayScore).length(); i < 43; i++) 
        std::cout << " ";
    std::cout << "║\n";
    std::cout << "╚════════════════════════════════════════════════╝\n\n";
}

void MatchEngine::PlaySelection() {
    // AI selects play from playbook - for now just random
}

void MatchEngine::DefenseLogic() {
    // Defensive strategy applied - influences defensive rating
}

void MatchEngine::ScoringLogic() {
    // Ball goes in/misses based on attributes - handled in SimulatePossession
}

void MatchEngine::ApplyFatigue() {
    // Update player stamina - future enhancement
}

void MatchEngine::CheckFouls() {
    // Check if fouls occurred - handled in SimulatePossession
}

void MatchEngine::HandleSubstitutions() {
    // AI decides subs - future enhancement
}

float MatchEngine::CalculatePlaySuccessRate(const Player& player) {
    // NCAA basketball: Even bad teams score 60-75 points per game
    // Base rate higher, attributes have SMALLER impact on scoring
    // This makes games more balanced but still allows skill differentiation
    
    float baseRate = 0.55f; // Higher base: 55% FG% baseline for everyone
    
    // Attributes give SMALL bonuses (max +10% total, not +39% like before)
    // This means even 5-rated players can score, but 20-rated players score more consistently
    
    // Shooting is most important but smaller impact (max +4%)
    float shootingBonus = (player.attributes.shooting / 20.0f) * 0.04f;
    
    // Pace affects shot creation (max +2%)
    float paceBonus = (player.attributes.pace / 20.0f) * 0.02f;
    
    // Ball control (max +2%)
    float ballControlBonus = (player.attributes.ballControl / 20.0f) * 0.02f;
    
    // Technical (basketball IQ) (max +2%)
    float technicalBonus = (player.attributes.technical / 20.0f) * 0.02f;
    
    // Total range: 50% (base) to 60% (perfect attributes)
    // This is realistic: Bad teams ~48% FG, Good teams ~52% FG in reality
    return baseRate + shootingBonus + paceBonus + ballControlBonus + technicalBonus;
}

float MatchEngine::CalculateDefenseEffectiveness() {
    if (!matchState) return 0.15f;
    
    // Average defensive rating of team
    float avgDefense = 10.0f; // Default mid-range
    
    // In future, calculate from actual defensive players on court
    // For now, use a baseline
    return 0.10f + (avgDefense / 20.0f) * 0.10f; // 10-20% defensive impact
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

} // namespace engine
} // namespace bm
