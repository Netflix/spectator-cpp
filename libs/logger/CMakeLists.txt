add_library(spectator-logger INTERFACE)

target_include_directories(spectator-logger
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spectator-logger
    INTERFACE
    spectator-utils
    spdlog::spdlog
)