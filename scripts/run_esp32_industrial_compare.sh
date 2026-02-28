#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
PORT="${1:-/dev/ttyUSB1}"
FQBN="esp32:esp32:esp32"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-6}"
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-2}"
CAPTURE_HELPER="${ROOT_DIR}/scripts/capture_esp32_serial.py"
BASELINE_LOG="/tmp/zerokernel_esp32_industrial_baseline.log"
MODULE_LOG="/tmp/zerokernel_esp32_industrial_module.log"
BASELINE_BUILD_LOG="/tmp/zerokernel_esp32_industrial_baseline_build.log"
MODULE_BUILD_LOG="/tmp/zerokernel_esp32_industrial_module_build.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_esp32_industrial_restore.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

compile_and_upload() {
  local sketch_path="$1"
  local build_log="$2"
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" --board-options UploadSpeed=115200 \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_LEAN_NET" \
    --upload -p "${PORT}" \
    "${sketch_path}" 2>&1 | tee "${build_log}"
}

restore_safe() {
  echo "Restoring KernelIdentity on ${PORT}"
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" --board-options UploadSpeed=115200 \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --upload -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" \
    2>&1 | tee "${RESTORE_BUILD_LOG}" >/dev/null || true
}

capture_serial() {
  local output_file="$1"
  python3 "${CAPTURE_HELPER}" \
    --port "${PORT}" \
    --seconds "${CAPTURE_SECONDS}" \
    --start-delay "${CAPTURE_START_DELAY}" \
    --output "${output_file}"
}

extract_line() {
  local prefix="$1"
  local output_file="$2"
  strings "${output_file}" | grep "${prefix}" | tail -n 1
}

trap restore_safe EXIT

echo "Running ESP32 Industrial baseline on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/IndustrialLoopBaseline" \
  "${BASELINE_BUILD_LOG}"
capture_serial "${BASELINE_LOG}"
extract_line 'INDUSTRIAL' "${BASELINE_LOG}"

echo "Running ESP32 Industrial ZeroKernel on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/IndustrialLoopNode" \
  "${MODULE_BUILD_LOG}"
capture_serial "${MODULE_LOG}"
extract_line 'INDUSTRIAL' "${MODULE_LOG}"

python3 - "${BASELINE_LOG}" "${MODULE_LOG}" "${BASELINE_BUILD_LOG}" "${MODULE_BUILD_LOG}" <<'PY'
import re
import subprocess
import sys

base_path, node_path, base_build_path, node_build_path = sys.argv[1:5]


def parse_last(prefix, path):
    line = ""
    text = subprocess.check_output(["strings", path], text=True, errors="ignore")
    for raw in text.splitlines():
        if prefix in raw:
            line = raw.strip()
    if not line:
        raise SystemExit(f"Missing {prefix} line in {path}")
    metrics = {k: int(v) for k, v in re.findall(r"([a-z_]+)=([0-9]+)", line)}
    return line, metrics


def parse_resource(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    ram = re.search(
        r"Global variables use (\d+) bytes \((?:\d+)%\) of dynamic memory, leaving \d+ bytes for local variables. Maximum is (\d+) bytes.",
        text,
    )
    flash = re.search(
        r"Sketch uses (\d+) bytes \((?:\d+)%\) of program storage space. Maximum is (\d+) bytes.",
        text,
    )
    return {
        "ram": tuple(int(x) for x in ram.groups()) if ram else None,
        "flash": tuple(int(x) for x in flash.groups()) if flash else None,
    }


def describe(before, after):
    if not before or not after:
        return "n/a"
    return f"{before[0]}/{before[1]} -> {after[0]}/{after[1]}"


base_line, base = parse_last("INDUSTRIAL", base_path)
node_line, node = parse_last("INDUSTRIAL", node_path)
base_res = parse_resource(base_build_path)
node_res = parse_resource(node_build_path)

print("---- industrial compare ----")
print(base_line)
print(node_line)
print("ram_bytes:", describe(base_res["ram"], node_res["ram"]))
print("flash_bytes:", describe(base_res["flash"], node_res["flash"]))
for key in ("control_runs", "command_updates", "diag_runs", "fast_avg_lag_us", "fast_max_lag_us", "fast_miss"):
    print(f"{key}:", base.get(key, 0), "->", node.get(key, 0))
print("fault_windows:", base.get("fault_windows", 0), "->", node.get("safe_mode_entries", 0))
print("recoveries:", 0, "->", node.get("recoveries", 0))
PY
