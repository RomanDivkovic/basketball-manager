cmake_minimum_required(VERSION 3.20)
project(BMCore VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SQLite3 REQUIRED)

# Lua 5.5 is built from the sources in third_party/lua so the library does not depend on Homebrew
enable_language(C)
file(GLOB BM_LUA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua/*.c)
add_library(bm_lua STATIC ${BM_LUA_SOURCES})
target_include_directories(bm_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua)
set_target_properties(bm_lua PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
if(UNIX)
    target_compile_definitions(bm_lua PRIVATE LUA_USE_POSIX)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/engine/MatchEngine.cpp
    src/engine/TacticalManager.cpp
    src/engine/SeasonManager.cpp
    src/engine/CareerManager.cpp
    src/engine/SimulationManager.cpp
    src/engine/CourtVisualizer.cpp
    src/ai/AICoordinator.cpp
    src/ai/TacticAI.cpp
    src/db/DatabaseManager.cpp
    src/db/DatabaseLoader.cpp
    src/db/LeagueImport.cpp
    src/lua/LuaRulesEngine.cpp
    src/lua/LuaBindings.cpp
    src/stats/StatisticsEngine.cpp
    src/bindings/UnityBridge.cpp
    src/player/PlayerGenerator.cpp
)

# Header files
set(HEADERS
    include/engine/MatchEngine.h
    include/engine/TacticalManager.h
    include/engine/SeasonManager.h
    include/engine/CareerManager.h
    include/engine/SimulationManager.h
    include/engine/CourtVisualizer.h
    include/ai/AICoordinator.h
    include/ai/TacticAI.h
    include/db/DatabaseManager.h
    include/db/DatabaseLoader.h
    include/db/LeagueImport.h
    include/lua/LuaRulesEngine.h
    include/lua/LuaBindings.h
    include/stats/StatisticsEngine.h
    include/bindings/UnityBridge.h
    include/player/PlayerGenerator.h
)

# Create main library
add_library(BMCore SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(BMCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLITE3_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lua
)

# Lua is linked statically on macOS so the shipped library does not depend on Homebrew
set(BM_LUA_LINK ${LUA_LIBRARIES})
if(APPLE)
    find_library(BM_LUA_STATIC NAMES liblua.a HINTS /opt/homebrew/opt/lua/lib /usr/local/opt/lua/lib)
    if(BM_LUA_STATIC)
        set(BM_LUA_LINK ${BM_LUA_STATIC})
        message(STATUS "Linking Lua statically: ${BM_LUA_STATIC}")
    endif()
endif()

# Link libraries
target_link_libraries(BMCore PUBLIC
    SQLite3::SQLite3
)
target_link_libraries(BMCore PRIVATE bm_lua)

# Output to lib directory
set_target_properties(BMCore PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
