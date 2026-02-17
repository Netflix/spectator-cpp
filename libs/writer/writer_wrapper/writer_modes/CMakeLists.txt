add_library(spectator-writer-modes
    buffered_write/buffered_write_mode.cpp
    non_buffered_write/non_buffered_write_mode.cpp
    lock_free_buffered_write/lock_free_buffered_write_mode.cpp
)

target_include_directories(spectator-writer-modes
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/buffered_write
    ${CMAKE_CURRENT_SOURCE_DIR}/non_buffered_write
    ${CMAKE_CURRENT_SOURCE_DIR}/lock_free_buffered_write
)

target_link_libraries(spectator-writer-modes
    PUBLIC
    spectator-logger
    spectator-writer-types
)
