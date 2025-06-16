add_library(registry
    registry.cpp
)
target_include_directories(registry
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(registry
    PUBLIC
    spectator-config
    spectator-meter-id
    spectator-meter-types
    spectator-writer-config
    spectator-writer-wrapper
    
)
add_executable(registry-test test_registry.cpp)
target_link_libraries(registry-test PRIVATE
    GTest::gtest 
    GTest::gtest_main
    registry
	spectator-utils
)
add_test(NAME registry-test COMMAND registry-test)