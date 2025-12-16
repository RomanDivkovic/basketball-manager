cmake_minimum_required(VERSION 3.20)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Player Generator Tool
add_executable(PlayerGeneratorTool
    generate_players.cpp
)

# Link to BMCore library
target_link_libraries(PlayerGeneratorTool PRIVATE BMCore SQLite::SQLite3)

# Include directories
target_include_directories(PlayerGeneratorTool PRIVATE 
    ${CMAKE_SOURCE_DIR}/bm-core/include
)

# Set C++20 standard
set_target_properties(PlayerGeneratorTool PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
