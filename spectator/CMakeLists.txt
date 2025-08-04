# Create a monolithic registry library with all required sources
add_library(spectator-registry
    registry.cpp
    # Include all required source files directly
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id/meter_id.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/src/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config/writer_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/memory_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/udp_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/uds_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper/writer.cpp
)

target_include_directories(spectator-registry
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger
)

# Link only external dependencies, not internal spectator libraries
target_link_libraries(spectator-registry
    PUBLIC
    spdlog::spdlog
    Boost::boost
)

# Configure paths for combined static library
set(BUILD_DIR "${CMAKE_BINARY_DIR}") # or your specific build dir
set(LIB_NAME "libspectator-registry-combined.a")
set(SPECTATOR_LIB "${BUILD_DIR}/spectator/libspectator-registry.a")

# Find dependencies (only include libraries with actual runtime dependencies)
# Note: Boost extraction removed since it's mostly header-only templates
# Dynamically find spdlog and fmt static libraries

# Find spdlog and fmt static libraries (debug or release)
find_library(SPDLOG_LIB NAMES spdlog)
find_library(FMT_LIB NAMES fmt)

add_custom_command(
    OUTPUT ${BUILD_DIR}/spectator/${LIB_NAME}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUILD_DIR}/temp_extract
    COMMAND ${CMAKE_COMMAND} -E echo "Extracting spectator library: ${SPECTATOR_LIB}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${BUILD_DIR}/temp_extract ar x ${SPECTATOR_LIB}
    # Extract only the dependencies that have actual runtime symbols
    COMMAND ${CMAKE_COMMAND} -E echo "Extracting runtime dependencies"
    COMMAND test -f ${SPDLOG_LIB} && ${CMAKE_COMMAND} -E chdir ${BUILD_DIR}/temp_extract ar x ${SPDLOG_LIB} || echo "spdlog library not found: ${SPDLOG_LIB}"
    COMMAND test -f ${FMT_LIB} && ${CMAKE_COMMAND} -E chdir ${BUILD_DIR}/temp_extract ar x ${FMT_LIB} || echo "fmt library not found: ${FMT_LIB}"
    # Create combined library by explicitly listing all .o files
    COMMAND ${CMAKE_COMMAND} -E echo "Creating combined library"
    COMMAND find ${BUILD_DIR}/temp_extract -name "*.o" -exec ar rcs ${BUILD_DIR}/spectator/${LIB_NAME} {} +
    # Cleanup
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${BUILD_DIR}/temp_extract
    DEPENDS spectator-registry
    WORKING_DIRECTORY ${BUILD_DIR}
    COMMENT "Combining static libraries into ${LIB_NAME}"
)

add_custom_target(
    combine_static_libs ALL
    DEPENDS ${BUILD_DIR}/spectator/${LIB_NAME}
)

add_executable(registry-test test_registry.cpp)

target_link_libraries(registry-test PRIVATE
    GTest::gtest 
    GTest::gtest_main
    spectator-registry
    spectator-utils
)

add_test(NAME registry-test COMMAND registry-test)