#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
PORT="${1:-/dev/ttyUSB0}"
FQBN="esp8266:esp8266:d1_mini"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-8}"
LEVEL2_LOG="/tmp/zerokernel_wemos_level2.log"
BUILD_LOG="/tmp/zerokernel_wemos_level2_build.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_wemos_level2_restore_build.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

restore_safe() {
  echo "Restoring KernelIdentity on ${PORT}"
  "${ARDUINO_CLI}" compile \
    --fqbn "${FQBN}" \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --upload \
    -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" 2>&1 | tee "${RESTORE_BUILD_LOG}" >/dev/null || true
}

trap restore_safe EXIT

capture_serial() {
  local output_file="$1"
  stty -F "${PORT}" 115200 raw -echo
  sleep 1
  timeout "${CAPTURE_SECONDS}s" cat "${PORT}" > "${output_file}" || true
}

echo "Running Wemos level-2 stress on ${PORT}"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${PORT}" \
  "${ROOT_DIR}/examples/WemosStressLevel2" 2>&1 | tee "${BUILD_LOG}"

capture_serial "${LEVEL2_LOG}"
grep '^trace ' "${LEVEL2_LOG}" | tail -n 5 || true

python3 - "${BUILD_LOG}" "${LEVEL2_LOG}" <<'PY'
import re
import sys

build_path, log_path = sys.argv[1], sys.argv[2]

def read_text(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        return fh.read()

def parse_resource(text):
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)
    flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", text)
    return {
        "ram": tuple(int(x) for x in ram.groups()) if ram else None,
        "flash": tuple(int(x) for x in flash.groups()) if flash else None,
    }

def parse_level2(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    matches = re.findall(
        r"LEVEL2 runs=\d+ overruns=\d+ queue_drop=\d+ task_failures=\d+ "
        r"execution_overruns=\d+ deadline_miss=\d+",
        text,
    )
    if not matches:
        raise SystemExit(f"Missing LEVEL2 line in {path}")
    line = matches[-1]
    metrics = {}
    for key, value in re.findall(r"([a-z_]+)=([0-9]+)", line):
        metrics[key] = int(value)
    trace_count = len(re.findall(r"^trace ", text, flags=re.MULTILINE))
    return line, metrics, trace_count

resources = parse_resource(read_text(build_path))
line, metrics, trace_count = parse_level2(log_path)

print("---- level2 summary ----")
print(line)
if resources["ram"] and resources["flash"]:
    ram_used, ram_total = resources["ram"]
    flash_used, flash_total = resources["flash"]
    print("ram_bytes:", ram_used, "/", ram_total)
    print("flash_bytes:", flash_used, "/", flash_total)
print("trace_lines:", trace_count)
print("queue_drop:", metrics.get("queue_drop", 0))
print("execution_overruns:", metrics.get("execution_overruns", 0))
print("task_failures:", metrics.get("task_failures", 0))
print("deadline_miss:", metrics.get("deadline_miss", 0))
PY
