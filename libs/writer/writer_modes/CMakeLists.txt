add_library(spectator-writer-modes
    buffered_write/buffered_write_mode.cpp
    non_buffered_write/non_buffered_write_mode.cpp
thread_local_buffered_write/thread_local_buffered_write_mode.cpp
)

target_include_directories(spectator-writer-modes
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/buffered_write
    ${CMAKE_CURRENT_SOURCE_DIR}/non_buffered_write
${CMAKE_CURRENT_SOURCE_DIR}/thread_local_buffered_write
)

target_link_libraries(spectator-writer-modes
    PUBLIC
    spectator-logger
    spectator-writer-types
)

add_executable(test_thread_local_buffered_write_mode
    thread_local_buffered_write/test_thread_local_buffered_write_mode.cpp
)

target_link_libraries(test_thread_local_buffered_write_mode
    PRIVATE
    spectator-writer-modes
    spectator-writer-types
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME thread_local_buffered_write_mode_test COMMAND test_thread_local_buffered_write_mode)
