cmake_minimum_required(VERSION 3.13)

project(spectator-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")
add_link_options(-fno-omit-frame-pointer "$<$<CONFIG:Debug>:-fsanitize=address>")

include(CTest)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

#-- spectator_test test executable
file(GLOB spectator_test_source_files
    "spectator/*_test.cc"
    "spectator/test_*.cc"
    "spectator/test_*.h"
)
add_executable(spectator_test ${spectator_test_source_files})
target_link_libraries(spectator_test spectator ${CONAN_LIBS})
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
)
target_link_libraries(spectator ${CONAN_LIBS})
target_link_options(spectator PRIVATE "$<$<CONFIG:Release>:-static-libstdc++>")
