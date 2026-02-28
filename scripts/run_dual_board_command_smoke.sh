#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
WEMOS_PORT="${1:-/dev/ttyUSB0}"
ESP32_PORT="${2:-/dev/ttyUSB1}"
WEMOS_FQBN="esp8266:esp8266:d1_mini"
ESP32_FQBN="esp32:esp32:esp32"
CAPTURE_SECONDS="${CAPTURE_SECONDS:-3}"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

restore_safe() {
  if [[ -e "${WEMOS_PORT}" ]]; then
    "${ARDUINO_CLI}" compile \
      --fqbn "${WEMOS_FQBN}" \
      --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
      --library "${ROOT_DIR}" \
      --upload \
      -p "${WEMOS_PORT}" \
      "${ROOT_DIR}/examples/KernelIdentity" >/dev/null 2>&1 || true
  fi

  if [[ -e "${ESP32_PORT}" ]]; then
    "${ARDUINO_CLI}" compile \
      --fqbn "${ESP32_FQBN}" \
      --board-options UploadSpeed=115200 \
      --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
      --library "${ROOT_DIR}" \
      --upload \
      -p "${ESP32_PORT}" \
      "${ROOT_DIR}/examples/KernelIdentity" >/dev/null 2>&1 || true
  fi
}

trap restore_safe EXIT

capture_summary() {
  local port="$1"
  local log_file="$2"
  stty -F "${port}" 115200 raw -echo
  timeout "${CAPTURE_SECONDS}s" cat "${port}" > "${log_file}" || true
  grep -E '^(CMD|EVENT|STATS)' "${log_file}" | tail -n 6 || true
}

echo "[wemos] command queue smoke"
"${ARDUINO_CLI}" compile \
  --fqbn "${WEMOS_FQBN}" \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${WEMOS_PORT}" \
  "${ROOT_DIR}/examples/CommandQueue"
capture_summary "${WEMOS_PORT}" "/tmp/zerokernel_wemos_command_smoke.log"

echo "[esp32] command queue smoke"
"${ARDUINO_CLI}" compile \
  --fqbn "${ESP32_FQBN}" \
  --board-options UploadSpeed=115200 \
  --library "${ROOT_DIR}" \
  --upload \
  -p "${ESP32_PORT}" \
  "${ROOT_DIR}/examples/CommandQueue"
capture_summary "${ESP32_PORT}" "/tmp/zerokernel_esp32_command_smoke.log"

echo "[safe] restoring KernelIdentity on both boards"
