add_library(spectator-writer-config STATIC
    writer_config.cpp
    writer_config.h
)

target_include_directories(spectator-writer-config
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)


target_link_libraries(spectator-writer-config
    PUBLIC
    spectator-writer-types
    spectator-logger
)

add_executable(test_writer_config 
    test_writer_config.cpp)

target_link_libraries(test_writer_config
    PRIVATE
    spectator-writer-config
    spectator-writer-types
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
)

add_test(NAME writer_config_test COMMAND test_writer_config)
