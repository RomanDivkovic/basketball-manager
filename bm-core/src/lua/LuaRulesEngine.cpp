#include "lua/LuaRulesEngine.h"
#include <lua.hpp>
#include <iostream>

namespace bm {
namespace lua {

LuaRulesEngine::LuaRulesEngine() : luaState(nullptr) {
}

LuaRulesEngine::~LuaRulesEngine() {
    if (luaState) {
        lua_close(luaState);
    }
}

bool LuaRulesEngine::Initialize() {
    luaState = luaL_newstate();
    if (!luaState) {
        std::cerr << "[LuaEngine] Failed to create Lua state\n";
        return false;
    }
    
    // Open standard libraries
    luaL_openlibs(luaState);
    
    std::cout << "[LuaEngine] Initialized with Lua " << LUA_VERSION << "\n";
    return true;
}

bool LuaRulesEngine::LoadLeagueRules(const std::string& rulesPath) {
    std::cout << "[LuaEngine] Loading league rules from " << rulesPath << "\n";
    
    int result = luaL_dofile(luaState, rulesPath.c_str());
    if (result != LUA_OK) {
        ReportError(lua_tostring(luaState, -1));
        return false;
    }
    
    loadedScripts[rulesPath] = "loaded";
    return true;
}

bool LuaRulesEngine::LoadModScript(const std::string& modPath) {
    std::cout << "[LuaEngine] Loading mod script from " << modPath << "\n";
    
    int result = luaL_dofile(luaState, modPath.c_str());
    if (result != LUA_OK) {
        ReportError(lua_tostring(luaState, -1));
        return false;
    }
    
    loadedScripts[modPath] = "loaded";
    return true;
}

bool LuaRulesEngine::GenerateSchedule(int season) {
    std::cout << "[LuaEngine] Generating schedule for season " << season << "\n";
    
    // Call Lua function
    lua_getglobal(luaState, "generateSchedule");
    if (!lua_isfunction(luaState, -1)) {
        std::cerr << "[LuaEngine] generateSchedule function not found\n";
        return false;
    }
    
    lua_pushinteger(luaState, season);
    
    int result = lua_pcall(luaState, 1, 0, 0);
    if (result != LUA_OK) {
        ReportError(lua_tostring(luaState, -1));
        return false;
    }
    
    return true;
}

bool LuaRulesEngine::GeneratePlayoffs(int season) {
    std::cout << "[LuaEngine] Generating playoffs for season " << season << "\n";
    
    lua_getglobal(luaState, "generatePlayoffs");
    if (!lua_isfunction(luaState, -1)) {
        std::cerr << "[LuaEngine] generatePlayoffs function not found\n";
        return false;
    }
    
    lua_pushinteger(luaState, season);
    
    int result = lua_pcall(luaState, 1, 0, 0);
    if (result != LUA_OK) {
        ReportError(lua_tostring(luaState, -1));
        return false;
    }
    
    return true;
}

bool LuaRulesEngine::CallFunction(const std::string& functionName) {
    lua_getglobal(luaState, functionName.c_str());
    if (!lua_isfunction(luaState, -1)) {
        std::cerr << "[LuaEngine] Function not found: " << functionName << "\n";
        return false;
    }
    
    int result = lua_pcall(luaState, 0, 0, 0);
    if (result != LUA_OK) {
        ReportError(lua_tostring(luaState, -1));
        return false;
    }
    
    return true;
}

void LuaRulesEngine::ReportError(const std::string& error) {
    std::cerr << "[LuaEngine ERROR] " << error << "\n";
}

} // namespace lua
} // namespace bm
