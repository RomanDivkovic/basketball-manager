#include "bindings/UnityBridge.h"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const char* databasePath = argc > 1 ? argv[1] : "/tmp/basketball_manager_readable.db";
    LoadDatabase(databasePath);

    const char* teams = GetTeamsJson();
    if (!teams || std::string(teams).find("\"teams\":[{") == std::string::npos) {
        std::cerr << "Bridge failed to return available teams\n";
        return EXIT_FAILURE;
    }

    InitializeMatch("1", "2");

    const char* initialState = GetMatchStateJson();
    if (!initialState || std::string(initialState) == "{}") {
        std::cerr << "Bridge failed to initialize a match\n";
        return EXIT_FAILURE;
    }

    SetSimulationSpeed(6);
    const int elapsed = SimulatePossession();
    const char* updatedState = GetMatchStateJson();
    if (elapsed <= 0 || !updatedState || std::string(updatedState) == "{}") {
        std::cerr << "Bridge failed to advance the match\n";
        return EXIT_FAILURE;
    }

    std::cout << updatedState << "\n";

    // Validate Season & Schedule bindings
    std::cout << "\n[Season Bridge Test] Initializing 2026/27 NBA Season...\n";
    InitializeSeason(2026);
    std::cout << "Calendar: " << GetSeasonCalendarJson() << "\n";
    std::cout << "Day 1 Fixtures: " << GetFixturesForDayJson(1) << "\n";
    std::cout << "Simulating Day 1...\n";
    int simResult = SimulateSeasonDay(6);
    std::cout << "SimulateDay result: " << simResult << "\n";
    std::cout << "Day 1 Fixtures (post-sim): " << GetFixturesForDayJson(1) << "\n";
    std::cout << "Eastern Standings: " << GetStandingsJson("Eastern") << "\n";

    // Validate Playoff system
    std::cout << "\n[Playoffs Bridge Test] Generating Playoff Bracket...\n";
    int bracketOk = GeneratePlayoffBracket();
    std::cout << "GeneratePlayoffBracket result: " << bracketOk << "\n";
    std::cout << "Playoffs Initial JSON: " << GetPlayoffsBracketJson() << "\n";
    std::cout << "Simulating Play-In round...\n";
    AdvancePlayoffRound(6);
    std::cout << "Playoffs post-round JSON: " << GetPlayoffsBracketJson() << "\n";

    return EXIT_SUCCESS;
}