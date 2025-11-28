#pragma once

#include <memory>

struct lua_State;

namespace bm {
namespace engine {
class MatchEngine;
class SimulationManager;
}

namespace lua {

/**
 * LuaBindings - C++ <-> Lua interop layer
 * Exposes C++ functions to Lua scripts
 */
class LuaBindings {
public:
    static void RegisterBindings(lua_State* L);

    // Binding functions (called from Lua)
    static int SimulatePossession(lua_State* L);
    static int GetMatchState(lua_State* L);
    static int ApplyPlayEffect(lua_State* L);
    static int ModifyPlayerAttribute(lua_State* L);
    static int LogMessage(lua_State* L);

private:
    static std::shared_ptr<engine::MatchEngine> matchEngine;
    static std::shared_ptr<engine::SimulationManager> simulationManager;

    // Helper functions
    static void PushTable(lua_State* L);
};

} // namespace lua
} // namespace bm
