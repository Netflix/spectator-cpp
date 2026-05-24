add_executable(performance_test performance_test.cpp)
target_link_libraries(performance_test PRIVATE
    spectator-registry
)

add_executable(meter_benchmark meter_benchmark.cpp)
target_link_libraries(meter_benchmark PRIVATE
    spectator-registry
)