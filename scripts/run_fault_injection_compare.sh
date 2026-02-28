#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
PORT="${1:-/dev/ttyUSB0}"
FQBN="${2:-esp8266:esp8266:d1_mini}"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-8}"
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-0}"
BASELINE_LOG="/tmp/zerokernel_fault_baseline.log"
KERNEL_LOG="/tmp/zerokernel_fault_kernel.log"
BASELINE_BUILD_LOG="/tmp/zerokernel_fault_baseline_build.log"
KERNEL_BUILD_LOG="/tmp/zerokernel_fault_kernel_build.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_fault_restore_build.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

BOARD_OPTIONS=()
if [[ "${FQBN}" == esp32:* ]]; then
  BOARD_OPTIONS+=(--board-options UploadSpeed=115200)
fi

compile_and_upload() {
  local sketch_path="$1"
  local build_log="$2"
  shift 2 || true
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" "${BOARD_OPTIONS[@]}" "$@" \
    --upload -p "${PORT}" "${sketch_path}" 2>&1 | tee "${build_log}"
}

restore_safe() {
  echo "Restoring KernelIdentity on ${PORT}"
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" "${BOARD_OPTIONS[@]}" \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --upload -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" \
    2>&1 | tee "${RESTORE_BUILD_LOG}" >/dev/null || true
}

capture_serial() {
  local output_file="$1"
  sleep "${CAPTURE_START_DELAY}"
  stty -F "${PORT}" 115200 raw -echo
  timeout "${CAPTURE_SECONDS}s" cat "${PORT}" > "${output_file}" || true
}

trap restore_safe EXIT

echo "Running fault baseline on ${PORT}"
compile_and_upload "${ROOT_DIR}/examples/FaultInjectionBaseline" "${BASELINE_BUILD_LOG}"
capture_serial "${BASELINE_LOG}"
grep 'BASELINE_FAULT' "${BASELINE_LOG}" | tail -n 1

echo "Running ZeroKernel fault demo on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/FaultInjectionDemo" \
  "${KERNEL_BUILD_LOG}" \
  --library "${ROOT_DIR}"
capture_serial "${KERNEL_LOG}"
grep 'ZEROKERNEL_FAULT' "${KERNEL_LOG}" | tail -n 1

python3 - "${BASELINE_LOG}" "${KERNEL_LOG}" "${BASELINE_BUILD_LOG}" "${KERNEL_BUILD_LOG}" <<'PY'
import re
import sys

base_path, kernel_path, base_build_path, kernel_build_path = sys.argv[1:5]

def parse_last(prefix, path):
    line = ""
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for raw in fh:
            if raw.startswith(prefix):
                line = raw.strip()
    if not line:
        raise SystemExit(f"Missing {prefix} line in {path}")
    metrics = {}
    for key, value in re.findall(r"([a-z_]+)=([A-Z_0-9]+)", line):
        if value.isdigit():
            metrics[key] = int(value)
        else:
            metrics[key] = value
    return line, metrics

def parse_ram(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    ram = re.search(r"Global variables use (\d+) bytes \((?:\d+)%\) of dynamic memory, leaving \d+ bytes for local variables. Maximum is (\d+) bytes.", text)
    if ram:
        return tuple(int(x) for x in ram.groups())
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)
    return tuple(int(x) for x in ram.groups()) if ram else None

baseline_line, baseline = parse_last("BASELINE_FAULT", base_path)
kernel_line, kernel = parse_last("ZEROKERNEL_FAULT", kernel_path)
baseline_ram = parse_ram(base_build_path)
kernel_ram = parse_ram(kernel_build_path)

def describe(before, after):
    if not before or not after:
        return "n/a"
    return f"{before[0]}/{before[1]} -> {after[0]}/{after[1]}"

print("---- fault compare ----")
print(baseline_line)
print(kernel_line)
print("ram_bytes:", describe(baseline_ram, kernel_ram))
print("fast_avg_lag_us:", baseline.get("fast_avg_lag_us", 0), "->", kernel.get("fast_avg_lag_us", 0))
print("fast_max_lag_us:", baseline.get("fast_max_lag_us", 0), "->", kernel.get("fast_max_lag_us", 0))
print("fast_miss:", baseline.get("fast_miss", 0), "->", kernel.get("fast_miss", 0))
print("failures:", baseline.get("failures", 0), "->", kernel.get("failures", 0))
print("recoveries:", baseline.get("recoveries", 0), "->", kernel.get("recoveries", 0))
print("state:", baseline.get("state", "n/a"), "->", kernel.get("state", "n/a"))
PY
