#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_BIN="/tmp/zerokernel_desktop_lean_smoke"
OUTPUT_DIR="/tmp/zerokernel_desktop_lean_smoke_build"
COMMON_FLAGS=(
  -std=c++11
  -DZEROKERNEL_PROFILE_POWER_SAVE
  -DZEROKERNEL_ENABLE_DIAGNOSTICS=0
  -DZEROKERNEL_ENABLE_DEBUG_DUMP=0
  -I "${ROOT_DIR}/src"
)

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

cc -std=c99 -DZEROKERNEL_PROFILE_POWER_SAVE -DZEROKERNEL_ENABLE_DIAGNOSTICS=0 \
  -DZEROKERNEL_ENABLE_DEBUG_DUMP=0 -I "${ROOT_DIR}/src" -c \
  "${ROOT_DIR}/src/internal/KernelHash.c" \
  -o "${OUTPUT_DIR}/KernelHash.o"

cc -DZEROKERNEL_PROFILE_POWER_SAVE -DZEROKERNEL_ENABLE_DIAGNOSTICS=0 \
  -DZEROKERNEL_ENABLE_DEBUG_DUMP=0 -I "${ROOT_DIR}/src" -c \
  "${ROOT_DIR}/src/internal/KernelArchAsm.S" \
  -o "${OUTPUT_DIR}/KernelArchAsm.o"

g++ "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/desktop/LeanProfileSmoke.cpp" \
  "${ROOT_DIR}/src/core/KernelRuntime.cpp" \
  "${ROOT_DIR}/src/core/KernelIdentity.cpp" \
  "${ROOT_DIR}/src/core/TaskRegistry.cpp" \
  "${ROOT_DIR}/src/core/EventHub.cpp" \
  "${ROOT_DIR}/src/core/Watchdog.cpp" \
  "${ROOT_DIR}/src/diagnostics/KernelDiagnostics.cpp" \
  "${ROOT_DIR}/src/core/KernelUtils.cpp" \
  "${ROOT_DIR}/src/internal/KernelString.cpp" \
  "${OUTPUT_DIR}/KernelHash.o" \
  "${OUTPUT_DIR}/KernelArchAsm.o" \
  -o "${OUTPUT_BIN}"

"${OUTPUT_BIN}"
