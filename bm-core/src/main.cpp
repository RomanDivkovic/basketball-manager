#include "engine/MatchEngine.h"
#include "engine/SimulationManager.h"
#include "db/DatabaseManager.h"
#include "db/DatabaseLoader.h"
#include "lua/LuaRulesEngine.h"
#include <iostream>
#include <memory>

namespace bm {

// Global game instance
std::shared_ptr<engine::SimulationManager> g_simulationManager;
std::shared_ptr<db::DatabaseManager> g_database;
std::shared_ptr<lua::LuaRulesEngine> g_luaEngine;

/**
 * Initialize Basketball Manager engine
 */
bool InitializeEngine(const char* dataPath) {
    try {
        std::cout << "[BM] Initializing Basketball Manager Engine\n";

        // Initialize database
        g_database = std::make_shared<db::DatabaseManager>();
        if (!g_database->Open(std::string(dataPath) + "/bm-game.db")) {
            std::cerr << "[BM ERROR] Failed to open database\n";
            return false;
        }

        // Initialize database schema
        if (!g_database->InitializeSchema()) {
            std::cerr << "[BM ERROR] Failed to initialize database schema\n";
            return false;
        }

        // Initialize Lua rules engine
        g_luaEngine = std::make_shared<lua::LuaRulesEngine>();
        if (!g_luaEngine->Initialize()) {
            std::cerr << "[BM ERROR] Failed to initialize Lua engine\n";
            return false;
        }

        // Load NCAA 2025 data
        auto loader = std::make_shared<db::DatabaseLoader>(g_database);
        if (!loader->LoadNCAATeams(std::string(dataPath) + "/ncaa-2025")) {
            std::cerr << "[BM ERROR] Failed to load NCAA teams\n";
            return false;
        }

        // Initialize simulation manager
        g_simulationManager = std::make_shared<engine::SimulationManager>();
        g_simulationManager->InitializeSeason(2025);

        std::cout << "[BM] Engine initialized successfully\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BM ERROR] Initialization failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * Shutdown engine
 */
void ShutdownEngine() {
    g_simulationManager.reset();
    g_luaEngine.reset();
    if (g_database) {
        g_database->Close();
        g_database.reset();
    }
}

} // namespace bm

int main(int argc, char* argv[]) {
    const char* dataPath = argc > 1 ? argv[1] : "./bm-data";
    
    if (!bm::InitializeEngine(dataPath)) {
        std::cerr << "Failed to initialize engine\n";
        return 1;
    }

    // Example: Initialize 2025 NCAA season
    std::cout << "Basketball Manager initialized. Ready for UI integration.\n";

    bm::ShutdownEngine();
    return 0;
}
