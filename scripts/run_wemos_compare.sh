#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
PORT="/dev/ttyUSB0"
FQBN="esp8266:esp8266:d1_mini"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-7}"
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-2}"
ENFORCE_DETERMINISM=0
BASELINE_LOG="/tmp/zerokernel_wemos_baseline.log"
KERNEL_LOG="/tmp/zerokernel_wemos_kernel.log"
BASELINE_BUILD_LOG="/tmp/zerokernel_wemos_baseline_build.log"
KERNEL_BUILD_LOG="/tmp/zerokernel_wemos_kernel_build.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_wemos_restore_build.log"

for arg in "$@"; do
  if [[ "${arg}" == "--enforce-determinism" ]]; then
    ENFORCE_DETERMINISM=1
  elif [[ "${arg}" == /dev/* ]]; then
    PORT="${arg}"
  fi
done

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

compile_and_upload() {
  local sketch_path="$1"
  local build_log="$2"
  shift 2 || true
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" "$@" --upload -p "${PORT}" "${sketch_path}" \
    2>&1 | tee "${build_log}"
}

restore_safe() {
  echo "Restoring KernelIdentity on ${PORT}"
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" \
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

echo "Running baseline sketch on ${PORT}"
compile_and_upload "${ROOT_DIR}/examples/WemosStressBaseline" "${BASELINE_BUILD_LOG}"
capture_serial "${BASELINE_LOG}"
grep 'BASELINE' "${BASELINE_LOG}" | tail -n 1

echo "Running ZeroKernel sketch on ${PORT}"
compile_and_upload \
  "${ROOT_DIR}/examples/WemosStressZeroKernel" \
  "${KERNEL_BUILD_LOG}" \
  --library "${ROOT_DIR}" \
  --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE"
capture_serial "${KERNEL_LOG}"
grep 'ZEROKERNEL' "${KERNEL_LOG}" | tail -n 1

if [[ "${ENFORCE_DETERMINISM}" -eq 1 ]]; then
  python3 - "${BASELINE_LOG}" "${KERNEL_LOG}" "${BASELINE_BUILD_LOG}" "${KERNEL_BUILD_LOG}" --enforce-determinism <<'PY'
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
    for key, value in re.findall(r"([a-z_]+)=([0-9]+)", line):
        metrics[key] = int(value)
    return line, metrics

def parse_resource(path):
    text = ""
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)
    flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", text)
    return {
        "ram": tuple(int(x) for x in ram.groups()) if ram else None,
        "flash": tuple(int(x) for x in flash.groups()) if flash else None,
    }

baseline_line, baseline = parse_last("BASELINE", base_path)
kernel_line, kernel = parse_last("ZEROKERNEL", kernel_path)
baseline_resources = parse_resource(base_build_path)
kernel_resources = parse_resource(kernel_build_path)

def describe(before, after):
    if not before or not after:
        return "n/a"
    return f"{before[0]}/{before[1]} -> {after[0]}/{after[1]}"

print("---- compare ----")
print(baseline_line)
print(kernel_line)
print("ram_bytes:", describe(baseline_resources["ram"], kernel_resources["ram"]))
print("flash_bytes:", describe(baseline_resources["flash"], kernel_resources["flash"]))
print(
    "fast_avg_lag_us:",
    baseline.get("fast_avg_lag_us", 0),
    "->",
    kernel.get("fast_avg_lag_us", 0),
)
print(
    "fast_max_lag_us:",
    baseline.get("fast_max_lag_us", 0),
    "->",
    kernel.get("fast_max_lag_us", 0),
)
print(
    "fast_miss:",
    baseline.get("fast_miss", 0),
    "->",
    kernel.get("fast_miss", 0),
)
print(
    "deadline_miss:",
    baseline.get("deadline_miss", 0),
    "->",
    kernel.get("deadline_miss", 0),
)
print(
    "queue_drop:",
    baseline.get("queue_drop", 0),
    "->",
    kernel.get("queue_drop", 0),
)

if (
    kernel.get("fast_miss", 0) != 0
    or kernel.get("fast_avg_lag_us", 0) != 0
    or kernel.get("fast_max_lag_us", 0) != 0
):
    raise SystemExit(2)
PY
else
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
    for key, value in re.findall(r"([a-z_]+)=([0-9]+)", line):
        metrics[key] = int(value)
    return line, metrics

def parse_resource(path):
    text = ""
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)
    flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", text)
    return {
        "ram": tuple(int(x) for x in ram.groups()) if ram else None,
        "flash": tuple(int(x) for x in flash.groups()) if flash else None,
    }

baseline_line, baseline = parse_last("BASELINE", base_path)
kernel_line, kernel = parse_last("ZEROKERNEL", kernel_path)
baseline_resources = parse_resource(base_build_path)
kernel_resources = parse_resource(kernel_build_path)

def describe(before, after):
    if not before or not after:
        return "n/a"
    return f"{before[0]}/{before[1]} -> {after[0]}/{after[1]}"

print("---- compare ----")
print(baseline_line)
print(kernel_line)
print("ram_bytes:", describe(baseline_resources["ram"], kernel_resources["ram"]))
print("flash_bytes:", describe(baseline_resources["flash"], kernel_resources["flash"]))
print(
    "fast_avg_lag_us:",
    baseline.get("fast_avg_lag_us", 0),
    "->",
    kernel.get("fast_avg_lag_us", 0),
)
print(
    "fast_max_lag_us:",
    baseline.get("fast_max_lag_us", 0),
    "->",
    kernel.get("fast_max_lag_us", 0),
)
print(
    "fast_miss:",
    baseline.get("fast_miss", 0),
    "->",
    kernel.get("fast_miss", 0),
)
print(
    "deadline_miss:",
    baseline.get("deadline_miss", 0),
    "->",
    kernel.get("deadline_miss", 0),
)
print(
    "queue_drop:",
    baseline.get("queue_drop", 0),
    "->",
    kernel.get("queue_drop", 0),
)
PY
fi
