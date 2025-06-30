add_library(spectator-config 
    config.cpp
)

target_include_directories(spectator-config
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spectator-config
    PUBLIC
    spectator-logger
    spectator-writer-config
    spectator-utils
)

add_executable(config-tests
    test_config.cpp
)

target_link_libraries(config-tests PRIVATE 
    spectator-config 
    GTest::gtest 
    GTest::gtest_main
    spectator-writer-config
)

add_test(
    NAME config-tests
    COMMAND config-tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
