cmake_minimum_required(VERSION 3.15)
project(spectator-cpp VERSION 2.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build the test suite" ON)

# Find dependencies (handled by Conan)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)

# Build tests if enabled
if(BUILD_TESTS)
    enable_testing()
endif()

# Add subdirectories
add_subdirectory(libs)
add_subdirectory(spectator)
add_subdirectory(performance_tests)
