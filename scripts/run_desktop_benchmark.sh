#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_BIN="/tmp/zerokernel_desktop_benchmark"
OUTPUT_DIR="/tmp/zerokernel_desktop_benchmark_build"
ENFORCE_PERFORMANCE=0

for arg in "$@"; do
  if [[ "${arg}" == "--enforce-performance" ]]; then
    ENFORCE_PERFORMANCE=1
  fi
done

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

cc -std=c99 -I "${ROOT_DIR}/src" -c \
  "${ROOT_DIR}/src/internal/KernelHash.c" \
  -o "${OUTPUT_DIR}/KernelHash.o"

cc -I "${ROOT_DIR}/src" -c \
  "${ROOT_DIR}/src/internal/KernelArchAsm.S" \
  -o "${OUTPUT_DIR}/KernelArchAsm.o"

g++ -std=c++11 -O2 -I "${ROOT_DIR}/src" \
  "${ROOT_DIR}/benchmarks/desktop/KernelBenchmark.cpp" \
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

BENCHMARK_OUTPUT="$("${OUTPUT_BIN}")"
printf '%s\n' "${BENCHMARK_OUTPUT}"

if [[ "${ENFORCE_PERFORMANCE}" -eq 1 ]]; then
  python3 - <<'PY' "${BENCHMARK_OUTPUT}"
import re
import sys

text = sys.argv[1]
string_match = re.search(r"mode=string .* seconds=([0-9.]+)", text)
fast_match = re.search(r"mode=fast .* seconds=([0-9.]+)", text)
if not string_match or not fast_match:
    raise SystemExit("Missing benchmark output")

string_seconds = float(string_match.group(1))
fast_seconds = float(fast_match.group(1))
string_ok = string_seconds <= 0.020
fast_ok = fast_seconds <= 0.015
ordering_ok = fast_seconds <= string_seconds
overall_ok = string_ok and fast_ok and ordering_ok

print(
    f"[perf] string={'PASS' if string_ok else 'FAIL'} "
    f"fast={'PASS' if fast_ok else 'FAIL'} "
    f"ordering={'PASS' if ordering_ok else 'FAIL'} "
    f"overall={'PASS' if overall_ok else 'FAIL'}"
)

if not overall_ok:
    raise SystemExit(2)
PY
fi
