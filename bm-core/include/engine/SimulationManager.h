#pragma once

#include <memory>
#include <vector>
#include <map>

namespace bm {
namespace engine {

class MatchEngine;
struct Team;

/**
 * SimulationManager - Manages overall game simulation
 * Handles season progression, standings, player development
 */
class SimulationManager {
public:
    SimulationManager();
    ~SimulationManager();

    // Initialize season
    void InitializeSeason(int seasonYear);

    // Simulate single round of matches
    void SimulateRound(int roundNumber);

    // Simulate full season
    void SimulateFullSeason();

    // Get standings
    std::vector<std::shared_ptr<Team>> GetStandings() const;

    // Player progression
    void UpdatePlayerProgression();

    // Draft system
    void HandleDraft();

private:
    std::shared_ptr<MatchEngine> matchEngine;
    int currentSeason;
    int currentRound;
    std::vector<std::shared_ptr<Team>> allTeams;

    void ProgressPlayerDevelopment(std::shared_ptr<Team> team);
    void SimulateInjuries();
};

} // namespace engine
} // namespace bm
