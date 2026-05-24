#!/usr/bin/env bash
# Build the spectator-wasm package using Emscripten.
# Requires the Emscripten SDK to be activated first:
#   source /path/to/emsdk/emsdk_env.sh
set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WASM_SRC="${REPO_ROOT}/spectator/wasm/spectator_embind.cpp"
OUT_DIR="$(dirname "$0")"

# All spectator-cpp C++ source files (same set as CMakeLists SPECTATOR_SOURCES)
SOURCES=(
    "${REPO_ROOT}/spectator/registry.cpp"
    "${REPO_ROOT}/spectator/c_api/spectator_c.cpp"
    "${REPO_ROOT}/libs/config/config.cpp"
    "${REPO_ROOT}/libs/meter/meter_id/meter_id.cpp"
    "${REPO_ROOT}/libs/utils/src/util.cpp"
    "${REPO_ROOT}/libs/writer/writer_config/writer_config.cpp"
    "${REPO_ROOT}/libs/writer/writer_types/src/memory_writer.cpp"
    "${REPO_ROOT}/libs/writer/writer_wrapper/writer.cpp"
    # Note: udp_writer.cpp and uds_writer.cpp are excluded — browsers have no raw sockets.
)

# Include paths (mirror CMakeLists SPECTATOR_INCLUDE_DIRS)
INCLUDES=(
    -I"${REPO_ROOT}/spectator"
    -I"${REPO_ROOT}/spectator/c_api"
    -I"${REPO_ROOT}/libs/config"
    -I"${REPO_ROOT}/libs/meter/meter_id"
    -I"${REPO_ROOT}/libs/meter/meter_types/include"
    -I"${REPO_ROOT}/libs/utils/include"
    -I"${REPO_ROOT}/libs/writer/writer_config"
    -I"${REPO_ROOT}/libs/writer/writer_types/include"
    -I"${REPO_ROOT}/libs/writer/writer_wrapper"
    -I"${REPO_ROOT}/libs/logger"
)

# spdlog — fetch headers if not already present
SPDLOG_DIR="${REPO_ROOT}/thirdparty/spdlog/include"
if [ -d "${SPDLOG_DIR}" ]; then
    INCLUDES+=(-I"${SPDLOG_DIR}")
else
    echo "Warning: spdlog headers not found at ${SPDLOG_DIR}."
    echo "Install via: conan install . --output-folder cmake-build --build='*'"
    echo "Then set SPDLOG_INCLUDE appropriately, or run from cmake-build."
fi

emcc \
    "${WASM_SRC}" \
    "${SOURCES[@]}" \
    "${INCLUDES[@]}" \
    -std=c++20 \
    -O3 \
    --bind \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="SpectatorModule" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT="web,node" \
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" \
    -o "${OUT_DIR}/spectator.js"

echo "Built: ${OUT_DIR}/spectator.js + ${OUT_DIR}/spectator.wasm"
