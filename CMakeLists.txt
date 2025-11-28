cmake_minimum_required(VERSION 3.20)
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
# Optional: Tools
add_subdirectory(bm-tools)

# Export targets
export(TARGETS BMCore FILE BasketballManagerTargets.cmake)
