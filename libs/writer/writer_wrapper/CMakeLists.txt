add_library(spectator-writer-wrapper
    src/writer.cpp
    src/sink.cpp
    src/pipeline.cpp
    src/thread_session.cpp
    src/thread_buffer.cpp
    src/thread_registry.cpp
)

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

add_executable(writer_test
    test/test_writer.cpp
)

target_link_libraries(writer_test
    PRIVATE
    GTest::GTest
    GTest::Main
    spectator-writer-wrapper
    spectator-meter-types
    spectator-meter-id
    uds_server_lib
    udp_server_lib
    writer_test_support
)
add_test(NAME writer_test COMMAND writer_test)

foreach(test_name thread_buffer_test thread_registry_test)
    string(REPLACE "_test" "" source_name ${test_name})
    add_executable(${test_name} test/test_${source_name}.cpp)
    target_link_libraries(${test_name}
        PRIVATE
        GTest::GTest
        GTest::Main
        spectator-writer-wrapper
        writer_test_support
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
