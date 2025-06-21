add_library(spectator-meter-id
    meter_id.cpp
)

target_include_directories(spectator-meter-id
    PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(spectator-meter-id
    PUBLIC
    spectator-utils
)

add_executable(MeterID-test
    test_meter_id.cpp
)

target_link_libraries(MeterID-test PRIVATE
    GTest::gtest 
    GTest::gtest_main
    spectator-meter-id
)