cmake_minimum_required(VERSION 3.20)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Match Simulator Tool
add_executable(MatchSimulator
    simulate_match.cpp
)

# Link to BMCore library
target_link_libraries(MatchSimulator PRIVATE BMCore SQLite::SQLite3)

# Include directories
target_include_directories(MatchSimulator PRIVATE 
    ${CMAKE_SOURCE_DIR}/bm-core/include
)

# Set C++20 standard
set_target_properties(MatchSimulator PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
