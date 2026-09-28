#!/bin/sh
set -eu
cd "$(dirname "$0")"
vulkan=OFF
case "${1:-}" in
  '') ;;
  --vulkan) vulkan=ON ;;
  *) echo 'usage: build-host.sh [--vulkan]' >&2; exit 2 ;;
esac
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBK_WITH_VULKAN="$vulkan" -DBK_BUILD_TESTS=ON
cmake --build build --parallel "${BK_BUILD_JOBS:-8}"
