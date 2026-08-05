#!/usr/bin/env bash

set -e

# usage: ./build.sh [clean|clean --confirm|skiptest]

if [[ -z "$BUILD_DIR" ]]; then
  BUILD_DIR="cmake-build"
fi

if [[ -z "$BUILD_TYPE" ]]; then
  # Choose: Debug, Release, RelWithDebInfo and MinSizeRel. Use Debug for asan checking locally.
  BUILD_TYPE="Debug"
fi

BLUE="\033[0;34m"
RED="\033[0;31m"
NC="\033[0m"

if [[ "$1" == "clean" ]]; then
  echo -e "${BLUE}==== clean ====${NC}"
  rm -rf "$BUILD_DIR"
  if [[ "$2" == "--confirm" ]]; then
    # remove all packages from the conan cache, to allow swapping between Release/Debug builds
    conan remove "*" --confirm
  fi
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  if ! command -v gcc-15 &> /dev/null; then
    echo -e "${RED}ERROR: gcc-15 is required but not found${NC}"
    exit 1
  fi
  if ! command -v g++-15 &> /dev/null; then
    echo -e "${RED}ERROR: g++-15 is required but not found${NC}"
    exit 1
  fi
  export CC=gcc-15
  export CXX=g++-15
fi

echo -e "${BLUE}==== env configuration ====${NC}"
echo "BUILD_DIR=$BUILD_DIR"
echo "BUILD_TYPE=$BUILD_TYPE"
echo "CC=$CC"
echo "CXX=$CXX"

if [[ ! -f "$HOME/.conan2/profiles/default" ]]; then
  echo -e "${BLUE}==== create default profile ====${NC}"
  conan profile detect
fi

## Modify the default profile to set the compiler version and C++ standard
DEFAULT_PROFILE="$HOME/.conan2/profiles/default"
sed -i.bak -E \
  -e 's/^compiler\.version=.*/compiler.version=15.2/' \
  -e 's/^compiler\.cppstd=.*/compiler.cppstd=23/' \
  "$DEFAULT_PROFILE"
rm -f "$DEFAULT_PROFILE.bak"

if [[ ! -d $BUILD_DIR ]]; then
  echo -e "${BLUE}==== install required dependencies ====${NC}"
  conan install . --output-folder="$BUILD_DIR" --build="missing" --settings=build_type="$BUILD_TYPE"
fi

pushd "$BUILD_DIR"

echo -e "${BLUE}==== configure conan environment to access tools ====${NC}"
source conanbuild.sh

if [[ $OSTYPE == "darwin"* ]]; then
  export MallocNanoZone=0
fi

echo -e "${BLUE}==== generate build files ====${NC}"
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo -e "${BLUE}==== build ====${NC}"
cmake --build .

if [[ "$1" != "skiptest" ]]; then
  echo -e "${BLUE}==== test ====${NC}"
  GTEST_COLOR=1 ctest --verbose
fi

popd