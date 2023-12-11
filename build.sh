#!/usr/bin/env bash

BUILD_DIR=cmake-build
# Choose: Debug, Release, RelWithDebInfo and MinSizeRel
BUILD_TYPE=Debug

BLUE="\033[0;34m"
NC="\033[0m"

if [[ "$1" == "clean" ]]; then
  echo -e "${BLUE}==== clean ====${NC}"
  rm -rf $BUILD_DIR
  # remove all packages and binaries from the local cache, to allow swapping between Debug/Release builds
  conan remove '*' --force
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
  export CC=gcc-11
  export CXX=g++-11
fi

if [[ ! -d $BUILD_DIR ]]; then
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo -e "${BLUE}==== configure default profile ====${NC}"
    conan profile new default --detect
    conan profile update settings.compiler.libcxx=libstdc++11 default
  fi

  echo -e "${BLUE}==== install required dependencies ====${NC}"
  if [[ "$BUILD_TYPE" == "Debug" ]]; then
    conan install . --build --install-folder $BUILD_DIR --profile ./sanitized
  else
    conan install . --build=missing --install-folder $BUILD_DIR
  fi
fi

pushd $BUILD_DIR || exit 1

echo -e "${BLUE}==== generate build files ====${NC}"
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE || exit 1

echo -e "${BLUE}==== build ====${NC}"
cmake --build . || exit 1

if [[ "$1" != "skiptest" ]]; then
  echo -e "${BLUE}==== test ====${NC}"
  GTEST_COLOR=1 ctest --verbose
fi

popd || exit 1
