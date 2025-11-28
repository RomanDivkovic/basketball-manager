#include "lua/LuaBindings.h"
#include <lua.hpp>
#include <iostream>

namespace bm {
namespace lua {

std::shared_ptr<engine::MatchEngine> LuaBindings::matchEngine = nullptr;
std::shared_ptr<engine::SimulationManager> LuaBindings::simulationManager = nullptr;

void LuaBindings::RegisterBindings(lua_State* L) {
    std::cout << "[LuaBindings] Registering C++ bindings\n";
    
    lua_register(L, "SimulatePossession", SimulatePossession);
    lua_register(L, "GetMatchState", GetMatchState);
    lua_register(L, "ApplyPlayEffect", ApplyPlayEffect);
    lua_register(L, "ModifyPlayerAttribute", ModifyPlayerAttribute);
    lua_register(L, "LogMessage", LogMessage);
}

int LuaBindings::SimulatePossession(lua_State* L) {
    if (matchEngine) {
        matchEngine->SimulatePossession();
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

int LuaBindings::GetMatchState(lua_State* L) {
    PushTable(L);
    return 1;
}

int LuaBindings::ApplyPlayEffect(lua_State* L) {
    const char* playName = luaL_checkstring(L, 1);
    std::cout << "[LuaBindings] Applying play: " << playName << "\n";
    return 0;
}

int LuaBindings::ModifyPlayerAttribute(lua_State* L) {
    const char* playerId = luaL_checkstring(L, 1);
    const char* attribute = luaL_checkstring(L, 2);
    lua_Number delta = luaL_checknumber(L, 3);
    
    std::cout << "[LuaBindings] Modifying " << playerId << " " << attribute 
              << " by " << delta << "\n";
    return 0;
}

int LuaBindings::LogMessage(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    std::cout << "[Lua] " << msg << "\n";
    return 0;
}

void LuaBindings::PushTable(lua_State* L) {
    lua_newtable(L);
    
    lua_pushstring(L, "status");
    lua_pushstring(L, "running");
    lua_settable(L, -3);
}

} // namespace lua
} // namespace bm
