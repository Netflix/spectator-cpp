 # Create a shared UDP server library for tests
add_library(udp_server_lib OBJECT
    udp_server.cpp
)
target_include_directories(udp_server_lib
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(udp_server_lib
    PUBLIC
    UDP_SERVER_LIB_ONLY
)
target_link_libraries(udp_server_lib
    PUBLIC
    Boost::system
    pthread
)

# UDP Server executable
add_executable(udp_server 
    udp_server.cpp
)
target_include_directories(udp_server
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(udp_server
    PUBLIC
    Boost::system
)