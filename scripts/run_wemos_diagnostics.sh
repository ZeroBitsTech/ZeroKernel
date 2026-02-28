#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
PORT="${1:-/dev/ttyUSB0}"
FQBN="esp8266:esp8266:d1_mini"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-7}"
DIAG_LOG="/tmp/zerokernel_wemos_diag.log"
BUILD_LOG="/tmp/zerokernel_wemos_diag_build.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

capture_serial() {
  local output_file="$1"
  stty -F "${PORT}" 115200 raw -echo
  sleep 1
  timeout "${CAPTURE_SECONDS}s" cat "${PORT}" > "${output_file}" || true
}

echo "Running Wemos diagnostics on ${PORT}"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${PORT}" \
  "${ROOT_DIR}/examples/WemosDiagnosticConsole" 2>&1 | tee "${BUILD_LOG}"

capture_serial "${DIAG_LOG}"
grep '^IDENTITY ' "${DIAG_LOG}" | tail -n 1 || true
grep '^STATUS ' "${DIAG_LOG}" | tail -n 1 || true
grep '^task ' "${DIAG_LOG}" | tail -n 3 || true

python3 - "${BUILD_LOG}" "${DIAG_LOG}" <<'PY'
import re
import sys

build_path, log_path = sys.argv[1:3]

with open(build_path, "r", encoding="utf-8", errors="ignore") as fh:
    build_text = fh.read()
with open(log_path, "r", encoding="utf-8", errors="ignore") as fh:
    log_text = fh.read()

ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", build_text)
flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", build_text)
identity = re.findall(r"^IDENTITY .*$", log_text, flags=re.MULTILINE)
status = re.findall(r"^STATUS .*$", log_text, flags=re.MULTILINE)
task_lines = re.findall(r"^task .*$", log_text, flags=re.MULTILINE)

def describe(match):
    if not match:
        return "n/a"
    used = int(match.group(1))
    total = int(match.group(2))
    return f"{used}/{total}"

print("---- diagnostics summary ----")
if identity:
    print(identity[-1])
if status:
    print(status[-1])
print("ram_bytes:", describe(ram))
print("flash_bytes:", describe(flash))
print("task_lines:", len(task_lines))
PY
