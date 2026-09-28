#!/bin/sh
set -eu
cd "$(dirname "$0")"
cmake -S . -B build-switch -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/switch-toolchain.cmake" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-switch --parallel "${BK_BUILD_JOBS:-8}"
