cmake_minimum_required(VERSION 3.20)
project(BMTools)

# Data importer tool
add_executable(bm-data-importer
    data-importer/main.cpp
)

target_link_libraries(bm-data-importer PUBLIC
    sqlite3
)

# Mod validator tool
add_executable(bm-mod-validator
    mod-validator/main.cpp
)

target_link_libraries(bm-mod-validator PUBLIC
    sqlite3
)

# Set output directory
set_target_properties(bm-data-importer bm-mod-validator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
