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
BASELINE_LOG="/tmp/zerokernel_esp32_modules_baseline.log"
MODULE_LOG="/tmp/zerokernel_esp32_modules_module.log"
BASELINE_BUILD_LOG="/tmp/zerokernel_esp32_modules_baseline_build.log"
MODULE_BUILD_LOG="/tmp/zerokernel_esp32_modules_module_build.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_esp32_modules_restore_build.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

compile_and_upload() {
  local sketch_path="$1"
  local build_log="$2"
  shift 2 || true
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" --board-options UploadSpeed=115200 "$@" \
    --upload -p "${PORT}" "${sketch_path}" 2>&1 | tee "${build_log}"
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

trap restore_safe EXIT

echo "Running ESP32 modules baseline on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/NetModulesBaseline" \
  "${BASELINE_BUILD_LOG}" \
  --library "${ROOT_DIR}"
capture_serial "${BASELINE_LOG}"
grep 'BASELINE_NETMODULES' "${BASELINE_LOG}" | tail -n 1

echo "Running ESP32 modules demo on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/NetModulesDemo" \
  "${MODULE_BUILD_LOG}" \
  --library "${ROOT_DIR}"
capture_serial "${MODULE_LOG}"
grep 'ZEROKERNEL_NETMODULES' "${MODULE_LOG}" | tail -n 1

python3 - "${BASELINE_LOG}" "${MODULE_LOG}" "${BASELINE_BUILD_LOG}" "${MODULE_BUILD_LOG}" <<'PY'
import re
import sys

base_path, module_path, base_build_path, module_build_path = sys.argv[1:5]


def parse_last(prefix, path):
    line = ""
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        for raw in fh:
            if raw.startswith(prefix):
                line = raw.strip()
    if not line:
        raise SystemExit(f"Missing {prefix} line in {path}")
    metrics = {}
    for key, value in re.findall(r"([a-z_]+)=([0-9]+)", line):
        metrics[key] = int(value)
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


base_line, base = parse_last("BASELINE_NETMODULES", base_path)
module_line, mod = parse_last("ZEROKERNEL_NETMODULES", module_path)
base_res = parse_resource(base_build_path)
mod_res = parse_resource(module_build_path)

print("---- net modules compare ----")
print(base_line)
print(module_line)
print("ram_bytes:", describe(base_res["ram"], mod_res["ram"]))
print("flash_bytes:", describe(base_res["flash"], mod_res["flash"]))
print("sample_runs:", base.get("sample_runs", 0), "->", mod.get("sample_runs", 0))
print("fast_avg_lag_us:", base.get("fast_avg_lag_us", 0), "->", mod.get("fast_avg_lag_us", 0))
print("fast_max_lag_us:", base.get("fast_max_lag_us", 0), "->", mod.get("fast_max_lag_us", 0))
print("fast_miss:", base.get("fast_miss", 0), "->", mod.get("fast_miss", 0))
print("wifi_attempts:", base.get("wifi_attempts", 0), "->", mod.get("wifi_attempts", 0))
print("wifi_reconnects:", base.get("wifi_reconnects", 0), "->", mod.get("wifi_reconnects", 0))
print("http_ok:", base.get("http_ok", 0), "->", mod.get("http_ok", 0))
print("http_fail:", base.get("http_fail", 0), "->", mod.get("http_fail", 0))
print("mqtt_ok:", base.get("mqtt_ok", 0), "->", mod.get("mqtt_ok", 0))
print("mqtt_fail:", base.get("mqtt_fail", 0), "->", mod.get("mqtt_fail", 0))
print("queue_max:", base.get("queue_max", 0), "->", mod.get("queue_max", 0))
PY
