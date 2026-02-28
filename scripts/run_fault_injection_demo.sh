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
LOG_FILE="/tmp/zerokernel_fault_injection_demo.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

BOARD_OPTIONS=()
if [[ "${FQBN}" == esp32:* ]]; then
  BOARD_OPTIONS+=(--board-options UploadSpeed=115200)
fi

restore_safe() {
  "${ARDUINO_CLI}" compile \
    --fqbn "${FQBN}" \
    "${BOARD_OPTIONS[@]}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --library "${ROOT_DIR}" \
    --upload \
    -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" >/dev/null 2>&1 || true
}

trap restore_safe EXIT

echo "[demo] fault injection on ${PORT} (${FQBN})"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  "${BOARD_OPTIONS[@]}" \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${PORT}" \
  "${ROOT_DIR}/examples/FaultInjectionDemo"

sleep "${CAPTURE_START_DELAY}"
stty -F "${PORT}" 115200 raw -echo
timeout "${CAPTURE_SECONDS}s" cat "${PORT}" > "${LOG_FILE}" || true
grep -E '^(STATE|SIGNAL|FAULT|HEALTH|REPORT|RECOVERY)' "${LOG_FILE}" | sed -n '1,40p' || true
