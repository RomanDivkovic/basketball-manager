cmake_minimum_required(VERSION 3.20)
project(BMTools)

# Find JsonCpp for mod validator
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# Data importer tool
add_executable(bm-data-importer
    data-importer/main.cpp
)

target_link_libraries(bm-data-importer PUBLIC
    sqlite3
)

# Mod validator tool (temporarily disabled due to jsoncpp linking issue)
# if(JSONCPP_FOUND)
#     add_executable(bm-mod-validator
#         mod-validator/main.cpp
#     )
#
#     target_link_libraries(bm-mod-validator PUBLIC
#         sqlite3
#         ${JSONCPP_LIBRARIES}
#     )
#     
#     target_include_directories(bm-mod-validator PRIVATE
#         ${JSONCPP_INCLUDE_DIRS}
#     )
# else()
#     message(STATUS "JsonCpp not found - mod-validator will be skipped")
#     message(STATUS "Install with: brew install jsoncpp")
# endif()

# Player generator tool
add_subdirectory(player-generator)

# Match simulator tool
add_subdirectory(match-simulator)

# Set output directory
set_target_properties(bm-data-importer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
