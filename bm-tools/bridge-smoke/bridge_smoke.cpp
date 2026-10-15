#include "bindings/UnityBridge.h"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const char* databasePath = argc > 1 ? argv[1] : "/tmp/basketball_manager_readable.db";
    LoadDatabase(databasePath);
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
    return EXIT_SUCCESS;
}