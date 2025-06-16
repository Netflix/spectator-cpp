add_library(spectator-writer-wrapper
    writer.cpp
)

target_link_libraries(spectator-writer-wrapper
    PUBLIC
    spectator-logger
    spectator-writer-types
)