#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build_dir=${ICSM_SERVER_BUILD_DIR:-"$root/build"}
job_count=${ICSM_BUILD_JOBS:-4}
args=()
case "$(uname -m)" in
  x86_64) export CC=clang CXX=clang++ ;;
  aarch64|arm64) args+=("-DCMAKE_TOOLCHAIN_FILE=$root/cross-x86_64.cmake") ;;
  *) echo 'Build on Ubuntu x86_64 or an Ubuntu ARM64 cross-builder.' >&2; exit 1 ;;
esac
cmake -S "$root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_M3=ON -DBUILD_M4=OFF -DBUILD_M5=OFF -DBUILD_M6=OFF \
  -DBUILD_PRIVATE_INVENTORY=ON "${args[@]}"
cmake --build "$build_dir" --parallel "$job_count" --target csgo_dedicated server engine \
  vphysics filesystem_stdio matchmaking_ds materialsystem scenefilecache \
  shaderapiempty soundemittersystem studiorender datacache vscript
printf 'Linux x86_64 server binaries: %s/bin\n' "$build_dir"
