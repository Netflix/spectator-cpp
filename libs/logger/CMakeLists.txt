add_library(spectator-logger logger.cpp)

target_include_directories(spectator-logger
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(spectator-logger
    PUBLIC
    spectator-utils
    PRIVATE
    spdlog::spdlog
)