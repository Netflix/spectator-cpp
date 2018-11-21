#!/bin/bash

set -e

RED='\033[0;31m' # Red
BB='\033[0;34m'  # Blue
NC='\033[0m' # No Color
BG='\033[0;32m' # Green

error() { >&2 echo -e "${RED}$1${NC}"; }
showinfo() { echo -e "${BG}$1${NC}"; }
workingprocess() { echo -e "${BB}$1${NC}"; }
allert () { echo -e "${RED}$1${NC}"; }

# Building project
mkdir -p build
cd build

if [ "$CC" = gcc ] ; then
  export CC=gcc-5
  export CXX=g++-5
fi

if [ "$CC" = gcc-5 ] ; then
  cmake -DCMAKE_BUILD_TYPE=Debug ..
else
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DASAN=ON ..
fi

make -j4
# Checks if last comand didn't output 0
# $? checks what last command outputed
# If output is 0 then command is succesfuly executed
# If command fails it outputs number between 0 to 255
if [ $? -ne 0 ]; then
    error "Error: there are compile errors!"
	# Terminate script and outputs 3
    exit 3
fi

showinfo "Running tests ..."

if [ "$CC" = gcc-5 ]; then
  make -j4 spectator_cpp_coverage
fi

./runtests
if [ $? -ne 0 ]; then
    error "Error: there are failed tests!"
    exit 4
fi

workingprocess "All tests compile and pass."

