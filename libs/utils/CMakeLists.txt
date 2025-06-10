add_library(spectator-utils STATIC
    src/util.cpp
    include/singleton.h
    include/util.h
)

target_include_directories(spectator-utils
    PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(spectator-utils
	PUBLIC
	spectator-meter-id
)