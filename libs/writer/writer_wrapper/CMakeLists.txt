add_library(spectator-writer-wrapper
    src/writer.cpp
    src/writer_state.cpp
)

# writer.h is the only public header. writer_state.h in src/ is private to the library.
target_include_directories(spectator-writer-wrapper
    PUBLIC
    include
)

target_link_libraries(spectator-writer-wrapper
    PUBLIC
    spectator-logger
    spectator-writer-types
    Threads::Threads
)

# WriterTestHelper, for tests that reach the Writer's private API. Only an include path: spectator-registry compiles
# the writer sources itself, so linking the wrapper library as well would define them twice.
add_library(spectator-writer-test-support INTERFACE)
target_include_directories(spectator-writer-test-support
    INTERFACE
    test
)

add_executable(writer_test
    test/test_writer.cpp
)

target_link_libraries(writer_test
    PRIVATE
    GTest::GTest
    GTest::Main
    spectator-writer-wrapper
    spectator-writer-test-support
    spectator-meter-types
    spectator-meter-id
    uds_server_lib
    udp_server_lib
    writer_test_support
)
add_test(NAME writer_test COMMAND writer_test)

add_executable(writer_state_test
    test/test_writer_state.cpp
)

target_include_directories(writer_state_test
    PRIVATE
    src
)

target_link_libraries(writer_state_test
    PRIVATE
    GTest::GTest
    GTest::Main
    spectator-writer-wrapper
    writer_test_support
)
add_test(NAME writer_state_test COMMAND writer_state_test)
