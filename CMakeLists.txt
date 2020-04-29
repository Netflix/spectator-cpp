cmake_minimum_required(VERSION 2.8)

set(PROJECT_NAME_STR spectator_cpp)
set(CMAKE_MACOSX_RPATH 1)
project(${PROJECT_NAME_STR})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

option (ASAN
        "Build with address sanitizer enabled (ON) or disabled (OFF) - 2x slowdown when enabled"
        OFF)

# Code coverage for debug build
string(TOLOWER ${CMAKE_BUILD_TYPE} LC_CMAKE_BUILD_TYPE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DCURL_STATICLIB -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -Werror -fno-rtti -Wall -Wno-missing-braces")

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU AND LC_CMAKE_BUILD_TYPE STREQUAL debug)
  MESSAGE(STATUS "Setting up CodeCoverage")
  include(CodeCoverage)
  setup_target_for_coverage(${PROJECT_NAME}_coverage runtests coverage)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
else()
  if (ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  endif()
endif()

include_directories(3rd-party ${CMAKE_CURRENT_BINARY_DIR})

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    link_directories(/usr/lib/x86_64-linux-gnu)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(gen_perc_bucket_tags ./gen_perc_bucket_tags.cc)
add_executable(gen_perc_bucket_values ./gen_perc_bucket_values.cc)

# add the commands to generate the include files
add_custom_command (
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
        COMMAND gen_perc_bucket_tags ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
        DEPENDS gen_perc_bucket_tags
)
add_custom_command (
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
        COMMAND gen_perc_bucket_values ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc
        DEPENDS gen_perc_bucket_values
)
add_custom_target(generated-files DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
        ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc)

file(GLOB LIB_SOURCE_FILES spectator/*.h spectator/*.cc)
add_library(spectator_cpp SHARED ${LIB_SOURCE_FILES}
        ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_tags.inc
        ${CMAKE_CURRENT_BINARY_DIR}/percentile_bucket_values.inc)
set_target_properties(spectator_cpp PROPERTIES COMPILE_FLAGS "-Wextra -Wno-missing-braces")
add_dependencies(spectator_cpp generated-files)

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcurl-linux.a libcurl.a COPYONLY)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcares-linux.a libcares.a COPYONLY)
  add_library(spectatorStatic STATIC ${LIB_SOURCE_FILES})
  add_dependencies(spectatorStatic generated-files)
  
  set_target_properties(spectatorStatic PROPERTIES OUTPUT_NAME spectator_cpp)
  target_link_libraries(spectatorStatic ${CMAKE_BINARY_DIR}/libcurl.a z)
else()
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcurl-osx.a libcurl.a COPYONLY)
  configure_file(${CMAKE_SOURCE_DIR}/bundle/libcares-osx.a libcares.a COPYONLY)
endif()
target_link_libraries(spectator_cpp ${CMAKE_BINARY_DIR}/libcurl.a ${CMAKE_BINARY_DIR}/libcares.a z)

# test configuration
enable_testing()
file(GLOB TEST_SOURCE_FILES test/*.cc test/*.h)

add_executable(runtests ${TEST_SOURCE_FILES})
set_target_properties(runtests PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare")

add_library(gtest 3rd-party/src/gtest-all.cc)

target_link_libraries(runtests spectator_cpp pthread gtest)
add_test(runtests runtests)

install(TARGETS spectator_cpp DESTINATION lib)
install(DIRECTORY spectator DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY 3rd-party/spdlog DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY 3rd-party/ska DESTINATION include FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY 3rd-party/tsl DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY 3rd-party/rapidjson DESTINATION include FILES_MATCHING PATTERN "*.h")

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  install(TARGETS spectatorStatic DESTINATION lib)
  install(FILES ${CMAKE_SOURCE_DIR}/bundle/libcurl-linux.a DESTINATION lib RENAME libspectator_curl.a)
  install(FILES ${CMAKE_SOURCE_DIR}/bundle/libcares-linux.a DESTINATION lib RENAME libspectator_cares.a)
endif()

