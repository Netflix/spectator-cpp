# Create a monolithic registry library with all required sources
add_library(spectator-registry
    registry.cpp
    # Include all required source files directly
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id/meter_id.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/src/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config/writer_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/memory_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/udp_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/uds_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper/writer.cpp
)

target_include_directories(spectator-registry
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger
)

# Link only external dependencies, not internal spectator libraries
target_link_libraries(spectator-registry
    PUBLIC
    spdlog::spdlog_header_only
    Boost::boost
    Boost::system
)
# Shared library exposing the C API for Go (CGo) and Python (ctypes/pybind11)
set(SPECTATOR_SOURCES
    registry.cpp
    c_api/spectator_c.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id/meter_id.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/src/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config/writer_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/memory_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/udp_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/src/uds_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper/writer.cpp
)

set(SPECTATOR_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/c_api
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger
)

# Shared library (for runtime use when static linking isn't required)
add_library(spectator-shared SHARED ${SPECTATOR_SOURCES})
target_include_directories(spectator-shared PUBLIC ${SPECTATOR_INCLUDE_DIRS})
target_link_libraries(spectator-shared PUBLIC spdlog::spdlog_header_only Boost::boost Boost::system)
set_target_properties(spectator-shared PROPERTIES OUTPUT_NAME "spectator")

# Fat static archive for Go (CGo) and any consumer that wants zero runtime dependencies.
# Boost::system object files are extracted and merged into libspectator_c.a so that
# consumers only need: -lspectator_c -lstdc++   (no separate -lboost_system required).
# spdlog is header-only so nothing to merge for it.
add_library(spectator-c-static STATIC ${SPECTATOR_SOURCES})
target_include_directories(spectator-c-static PUBLIC ${SPECTATOR_INCLUDE_DIRS})
target_link_libraries(spectator-c-static PUBLIC spdlog::spdlog_header_only Boost::boost Boost::system)
set_target_properties(spectator-c-static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME "spectator_c"
)

# Post-build: merge Boost::system object files into libspectator_c.a.
# We extract to a temp dir to avoid name collisions, then re-add to the archive.
set(_FAT_TMP "${CMAKE_CURRENT_BINARY_DIR}/_fat_tmp")
add_custom_command(TARGET spectator-c-static POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_FAT_TMP}
    COMMAND bash -c "cd ${_FAT_TMP} && ${CMAKE_AR} x $<TARGET_FILE:Boost::system>"
    COMMAND bash -c "${CMAKE_AR} qcs $<TARGET_FILE:spectator-c-static> ${_FAT_TMP}/*.o"
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${_FAT_TMP}
    COMMENT "Fat archive: merging Boost::system into libspectator_c.a"
    VERBATIM
)

# pybind11 Python extension (builds spectator_cpp.so importable from Python)
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(spectator_cpp python/spectator_pybind.cpp ${SPECTATOR_SOURCES})
    target_include_directories(spectator_cpp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/config
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_id
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/meter/meter_types/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_config
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_types/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/writer/writer_wrapper
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/logger
    )
    target_link_libraries(spectator_cpp PRIVATE spdlog::spdlog_header_only Boost::boost Boost::system)
    set_target_properties(spectator_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    message(STATUS "pybind11 not found — skipping Python extension target")
endif()

# Node.js N-API native addon (cmake-js injects CMAKE_JS_VERSION when active)
if(DEFINED CMAKE_JS_VERSION)
    add_library(spectator_cpp_node SHARED node/spectator_napi.cpp ${SPECTATOR_SOURCES})
    target_include_directories(spectator_cpp_node PRIVATE
        ${CMAKE_JS_INC}
        ${NODE_ADDON_API_DIR}
        ${SPECTATOR_INCLUDE_DIRS}
    )
    target_link_libraries(spectator_cpp_node PRIVATE
        ${CMAKE_JS_LIB}
        spdlog::spdlog_header_only
        Boost::boost
        Boost::system
    )
    set_target_properties(spectator_cpp_node PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME "spectator_cpp"
        POSITION_INDEPENDENT_CODE ON
    )
    message(STATUS "cmake-js detected — building Node.js N-API addon")
else()
    message(STATUS "cmake-js not detected — skipping Node.js addon target")
endif()

add_executable(registry-test test_registry.cpp)
target_link_libraries(registry-test PRIVATE
    GTest::gtest 
    GTest::gtest_main
    spectator-registry
    spectator-utils
)
add_test(NAME registry-test COMMAND registry-test)