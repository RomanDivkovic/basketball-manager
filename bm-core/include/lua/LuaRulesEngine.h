#pragma once

#include <memory>
#include <string>
#include <map>

struct lua_State;

namespace bm {
namespace lua {

/**
 * LuaRulesEngine - Manages league rules, scheduling, playoffs
 * Lua scripts for NCAA ruleset, mod integration
 */
class LuaRulesEngine {
public:
    LuaRulesEngine();
    ~LuaRulesEngine();

    // Initialize Lua state
    bool Initialize();

    // Load league ruleset
    bool LoadLeagueRules(const std::string& rulesPath);

    // Load mod scripts
    bool LoadModScript(const std::string& modPath);

    // Execute scheduling logic
    bool GenerateSchedule(int season);

    // Execute playoff logic
    bool GeneratePlayoffs(int season);

    // Call Lua function from C++
    bool CallFunction(const std::string& functionName);

    // Get Lua state (for bindings)
    lua_State* GetLuaState() const { return luaState; }

private:
    lua_State* luaState;
    std::map<std::string, std::string> loadedScripts;

    // Error handling
    void ReportError(const std::string& error);
};

} // namespace lua
} // namespace bm
