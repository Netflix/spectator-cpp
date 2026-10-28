# Header-only helpers shared by the writer tests, such as WaitFor.
add_library(writer_test_support INTERFACE)
target_include_directories(writer_test_support
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(writer_test_support
    INTERFACE
    spectator-writer-types
)

add_subdirectory(udp_server)
add_subdirectory(uds_server)
