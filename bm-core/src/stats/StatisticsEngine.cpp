#include "stats/StatisticsEngine.h"
#include <cmath>

namespace bm {
namespace stats {

StatisticsEngine::StatisticsEngine() {
}

StatisticsEngine::~StatisticsEngine() {
}

float StatisticsEngine::CalculateEFG(int fgm, int fga, int ftm) {
    if (fga == 0) return 0.0f;
    return (fgm + 0.5f * ftm) / fga;
}

float StatisticsEngine::CalculateTOVRate(int turnovers, int possessions) {
    if (possessions == 0) return 0.0f;
    return static_cast<float>(turnovers) / possessions;
}

float StatisticsEngine::CalculateREBRate(int rebounds, int missedShots) {
    if (missedShots == 0) return 0.0f;
    return static_cast<float>(rebounds) / missedShots;
}

float StatisticsEngine::CalculateFTRate(int ftm, int fga) {
    if (fga == 0) return 0.0f;
    return static_cast<float>(ftm) / fga;
}

float StatisticsEngine::CalculateOrtg(int pointsFor, int possessions) {
    if (possessions == 0) return 0.0f;
    return (pointsFor / static_cast<float>(possessions)) * 100.0f;
}

float StatisticsEngine::CalculateDrtg(int pointsAgainst, int possessions) {
    if (possessions == 0) return 0.0f;
    return (pointsAgainst / static_cast<float>(possessions)) * 100.0f;
}

float StatisticsEngine::CalculatePace(int fga, int fta, int turnovers, int minutes) {
    if (minutes == 0) return 0.0f;
    float possessions = fga + 0.4f * fta - 1.07f * (turnovers);
    return (possessions / minutes) * 40.0f;
}

float StatisticsEngine::CalculatePER(
    int points, int rebounds, int assists, int steals, int blocks,
    int fouls, int turnovers, int minutes, float leaguePER) {
    
    if (minutes == 0) return 0.0f;
    
    // Simplified PER calculation
    float per = (points + rebounds + assists + steals + blocks - 
                fouls - turnovers) / minutes;
    
    return per * (15.0f / leaguePER);
}

float StatisticsEngine::CalculateTS(int points, int fga, int fta) {
    if (fga + 0.44f * fta == 0) return 0.0f;
    return points / (2.0f * (fga + 0.44f * fta));
}

} // namespace stats
} // namespace bm
