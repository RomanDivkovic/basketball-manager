cmake_minimum_required(VERSION 3.20)

# Runs on macOS 11 and newer (must be set before project())
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(BasketballManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_DIALECT "C++20")

# Architecture detection for M1 Mac
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(IS_ARM64 TRUE)
    message(STATUS "Building for ARM64 (Apple Silicon)")
endif()

# Add subdirectories
add_subdirectory(bm-core)

# Optional: UI will be handled separately via Unity
# Tools and tests are only built on the desktop
option(BM_BUILD_TOOLS "Build the tests and tools" ON)
if(BM_BUILD_TOOLS)
    add_subdirectory(bm-tools)
    if(TARGET bm_sqlite)
        export(TARGETS BMCore bm_lua bm_sqlite FILE BasketballManagerTargets.cmake)
    else()
        export(TARGETS BMCore bm_lua FILE BasketballManagerTargets.cmake)
    endif()
endif()
