add_library(spectator-writer-wrapper
    writer.cpp
)

target_include_directories(spectator-writer-wrapper
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spectator-writer-wrapper
    PUBLIC
    spectator-logger
    spectator-writer-types
)

add_executable(writer_test
    test_writer.cpp
)

target_link_libraries(writer_test
    PRIVATE
    GTest::GTest
    GTest::Main
    spectator-writer-wrapper
    spectator-meter-types
    spectator-meter-id
    uds_server_lib
)
add_test(NAME writer_test COMMAND writer_test)