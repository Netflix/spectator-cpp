# Create a shared UDS server library for tests
add_library(uds_server_lib OBJECT
    uds_server.cpp
)
target_include_directories(uds_server_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(uds_server_lib
    PUBLIC
    UDS_SERVER_LIB_ONLY
)
target_link_libraries(uds_server_lib
    PUBLIC
    Boost::system
    pthread
)

# UDS Server executable
add_executable(uds_server 
    uds_server.cpp
)
target_include_directories(uds_server
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(uds_server
    PUBLIC
    Boost::system
)
