cmake_minimum_required(VERSION 3.15)
project(spectator-cpp LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-pedantic -Werror -Wall -Wno-missing-braces -fno-omit-frame-pointer)

# Find dependencies (handled by Conan)
find_package(spdlog REQUIRED)
find_package(GTest REQUIRED)
find_package(Boost REQUIRED)

include(CTest)

# Add subdirectories
add_subdirectory(libs)
add_subdirectory(spectator)
add_subdirectory(performance_tests)
