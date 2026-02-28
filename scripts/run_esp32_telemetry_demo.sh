#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi

PORT="${1:-/dev/ttyUSB1}"
FQBN="esp32:esp32:esp32"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-8}"
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-1}"
LOG_FILE="/tmp/zerokernel_esp32_telemetry_demo.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

restore_safe() {
  "${ARDUINO_CLI}" compile \
    --fqbn "${FQBN}" \
    --board-options UploadSpeed=115200 \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --library "${ROOT_DIR}" \
    --upload \
    -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" >/dev/null 2>&1 || true
}

trap restore_safe EXIT

echo "[esp32] telemetry node demo on ${PORT}"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --board-options UploadSpeed=115200 \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${PORT}" \
  "${ROOT_DIR}/examples/ESP32TelemetryNode"

sleep "${CAPTURE_START_DELAY}"
stty -F "${PORT}" 115200 raw -echo
timeout "${CAPTURE_SECONDS}s" cat "${PORT}" > "${LOG_FILE}" || true
grep -E '^(STATE|WIFI|SAMPLE|HEARTBEAT|DIAG|STATS|SIGNAL)' "${LOG_FILE}" | sed -n '1,24p' || true
