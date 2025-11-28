cmake_minimum_required(VERSION 3.20)
project(BMCore VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(SQLite3 REQUIRED)

# Lua dependency (will use Lua 5.4)
set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/src")
set(LUA_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua/liblua.a")

# Source files
set(SOURCES
    src/main.cpp
    src/engine/MatchEngine.cpp
    src/engine/SimulationManager.cpp
    src/ai/AICoordinator.cpp
    src/ai/TacticAI.cpp
    src/db/DatabaseManager.cpp
    src/db/DatabaseLoader.cpp
    src/lua/LuaRulesEngine.cpp
    src/lua/LuaBindings.cpp
    src/stats/StatisticsEngine.cpp
    src/bindings/UnityBridge.cpp
)

# Header files
set(HEADERS
    include/engine/MatchEngine.h
    include/engine/SimulationManager.h
    include/ai/AICoordinator.h
    include/ai/TacticAI.h
    include/db/DatabaseManager.h
    include/db/DatabaseLoader.h
    include/lua/LuaRulesEngine.h
    include/lua/LuaBindings.h
    include/stats/StatisticsEngine.h
    include/bindings/UnityBridge.h
)

# Create main library
add_library(BMCore SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(BMCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${SQLITE3_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(BMCore PUBLIC
    ${SQLITE3_LIBRARIES}
    ${LUA_LIBRARY}
)

# For macOS M1, set correct architecture
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    target_compile_options(BMCore PRIVATE -arch arm64)
    target_link_options(BMCore PRIVATE -arch arm64)
endif()

# Output to lib directory
set_target_properties(BMCore PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
