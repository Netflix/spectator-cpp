# Create a monolithic registry library with all required sources
add_library(spectator-registry STATIC
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger/logger
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
    PRIVATE
    spdlog::spdlog_header_only
    Boost::boost
)

add_executable(registry-test test_registry.cpp)
target_link_libraries(registry-test PRIVATE
    GTest::gtest 
    GTest::gtest_main
    spectator-registry
    spectator-utils
)
add_test(NAME registry-test COMMAND registry-test)
