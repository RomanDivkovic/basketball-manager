#pragma once

#include <string>
#include <vector>
#include <map>

namespace bm {
namespace stats {

/**
 * StatisticsEngine - Advanced basketball statistics
 * Calculates Four Factors (eFG%, TOV%, REB%, FT%), ORtg, DRtg, Pace, etc.
 */
class StatisticsEngine {
public:
    StatisticsEngine();
    ~StatisticsEngine();

    // Four Factors
    float CalculateEFG(int fgm, int fga, int ftm);
    float CalculateTOVRate(int turnovers, int possessions);
    float CalculateREBRate(int rebounds, int missedShots);
    float CalculateFTRate(int ftm, int fga);

    // Efficiency metrics
    float CalculateOrtg(int pointsFor, int possessions);
    float CalculateDrtg(int pointsAgainst, int possessions);

    // Pace calculation
    float CalculatePace(int fga, int fta, int turnovers, int minutes);

    // Player efficiency rating
    float CalculatePER(
        int points, int rebounds, int assists, int steals, int blocks,
        int fouls, int turnovers, int minutes, float leaguePER
    );

    // True shooting percentage
    float CalculateTS(int points, int fga, int fta);

private:
    const float LEAGUE_AVERAGE_PER = 15.0f;
};

} // namespace stats
} // namespace bm
