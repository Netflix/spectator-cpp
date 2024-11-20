cmake_minimum_required(VERSION 3.13)

project(spectator-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-pedantic -Werror -Wall -Wno-missing-braces -fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")

find_package(absl REQUIRED)
find_package(asio REQUIRED)
find_package(Backward REQUIRED)
find_package(fmt REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)

include(CTest)

#-- spectator_test test executable
file(GLOB spectator_test_source_files
    "spectator/*_test.cc"
    "spectator/test_*.cc"
    "spectator/test_*.h"
)
add_executable(spectator_test ${spectator_test_source_files})
target_link_libraries(spectator_test
    spectator
    gtest::gtest
)
add_test(
    NAME spectator_test
    COMMAND spectator_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#-- spectator library
add_library(spectator
    "spectator/logger.cc"
    "spectator/publisher.cc"
    "spectator/config.h"
    "spectator/id.h"
    "spectator/logger.h"
    "spectator/measurement.h"
    "spectator/meter_type.h"
    "spectator/publisher.h"
    "spectator/registry.h"
    "spectator/stateful_meters.h"
    "spectator/stateless_meters.h"
    "spectator/valid_chars.inc"
)
target_link_libraries(spectator
    abseil::abseil
    asio::asio
    Backward::Backward
    fmt::fmt
    spdlog::spdlog
)

#-- generator tools
add_executable(gen_valid_chars "tools/gen_valid_chars.cc")

#-- file generators, must exist where the outputs are referenced
add_custom_command(
    OUTPUT "spectator/valid_chars.inc"
    COMMAND "${CMAKE_BINARY_DIR}/bin/gen_valid_chars" > "${CMAKE_SOURCE_DIR}/spectator/valid_chars.inc"
    DEPENDS gen_valid_chars
)
