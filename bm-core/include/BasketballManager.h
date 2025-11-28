#pragma once

/**
 * Basketball Manager Engine - Main Public API
 */

// Version
#define BM_VERSION_MAJOR 1
#define BM_VERSION_MINOR 0
#define BM_VERSION_PATCH 0

// Common types
#include "common/Types.h"

// Engine headers
#include "engine/MatchEngine.h"
#include "engine/SimulationManager.h"

// AI headers
#include "ai/AICoordinator.h"
#include "ai/TacticAI.h"

// Database headers
#include "db/DatabaseManager.h"
#include "db/DatabaseLoader.h"

// Lua headers
#include "lua/LuaRulesEngine.h"
#include "lua/LuaBindings.h"

// Statistics headers
#include "stats/StatisticsEngine.h"

// Bindings
#include "bindings/UnityBridge.h"

namespace bm {

/**
 * Get engine version string
 */
inline const char* GetVersionString() {
    return "Basketball Manager 1.0.0-alpha (macOS M1)";
}

/**
 * Get build configuration
 */
inline const char* GetBuildInfo() {
    #ifdef NDEBUG
    return "Release Build";
    #else
    return "Debug Build";
    #endif
}

} // namespace bm
