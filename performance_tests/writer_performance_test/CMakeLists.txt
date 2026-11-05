# Measures recording through the writers for a list of configurations. See writer_performance_test.cpp.
add_executable(writer_performance_test
    writer_performance_test.cpp
    plan.cpp
    receiver.cpp
    recording_run.cpp
    report.cpp
)

target_link_libraries(writer_performance_test
    PRIVATE
    spectator-registry
)
