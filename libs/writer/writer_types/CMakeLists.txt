add_subdirectory(test_utils)

add_library(spectator-writer-types
    src/memory_writer.cpp
    src/udp_writer.cpp
    src/uds_writer.cpp
)

target_include_directories(spectator-writer-types
    PUBLIC
    include
)

target_link_libraries(spectator-writer-types
    PUBLIC
    spectator-logger
    Boost::boost
    Boost::system
)

set(TEST_SOURCES
    test/test_memory_writer.cpp
    test/test_udp_writer.cpp
    test/test_uds_writer.cpp
)


# Create individual test executables for each test file
foreach(test_file ${TEST_SOURCES})
    # Get the filename without extension and path
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # Create an executable for this test file
    add_executable(${test_name} ${test_file})
    
    # Include test directory and server directories
    target_include_directories(${test_name}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
        ${CMAKE_CURRENT_SOURCE_DIR}/test_utils/udp_server
        ${CMAKE_CURRENT_SOURCE_DIR}/test_utils/uds_server
    )
    
    # Link against GTest and other required libraries
    target_link_libraries(${test_name} PRIVATE 
        GTest::GTest 
        GTest::Main
        spectator-writer-types
        udp_server_lib
        uds_server_lib
    )
    
    # Add the test to CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

