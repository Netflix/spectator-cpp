add_library(spectator-meter-types INTERFACE)

target_include_directories(spectator-meter-types
    INTERFACE
    include
)

# List all the test files
set(TEST_SOURCES
    test/test_age_gauge.cpp
    test/test_counter.cpp
    test/test_dist_summary.cpp
    test/test_gauge.cpp
    test/test_max_gauge.cpp
    test/test_monotonic_counter.cpp
    test/test_monotonic_counter_uint.cpp
    test/test_percentile_dist_summary.cpp
    test/test_percentile_timer.cpp
    test/test_timer.cpp
)

# Create individual test executables for each test file
foreach(test_file ${TEST_SOURCES})
    # Get the filename without extension and path
    get_filename_component(test_name ${test_file} NAME_WE)
    
    # Create an executable for this test file
    add_executable(${test_name} ${test_file})
    
    # Link against GTest and other required libraries
    target_link_libraries(${test_name} PRIVATE 
        GTest::GTest 
        GTest::Main
        spectator-meter-types
        spectator-meter-id
        spectator-writer-wrapper
    )
    
    # Add the test to CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()