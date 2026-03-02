add_library(spectator-writer-modes
    src/write_mode.cpp
    src/buffered_write_mode.cpp
    src/non_buffered_write_mode.cpp
    src/thread_local_buffered_write_mode.cpp
)

target_include_directories(spectator-writer-modes
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(spectator-writer-modes
    PUBLIC
    spectator-logger
    spectator-writer-types
)

add_executable(test_thread_local_buffered_write_mode
    test/test_thread_local_buffered_write_mode.cpp
)

target_link_libraries(test_thread_local_buffered_write_mode
    PRIVATE
    spectator-writer-modes
    spectator-writer-types
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME thread_local_buffered_write_mode_test COMMAND test_thread_local_buffered_write_mode)
