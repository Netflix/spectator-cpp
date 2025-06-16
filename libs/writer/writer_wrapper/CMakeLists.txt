add_library(spectator-writer-wrapper
    writer.cpp
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
    uds_server_lib
)
add_test(NAME writer_test COMMAND writer_test)