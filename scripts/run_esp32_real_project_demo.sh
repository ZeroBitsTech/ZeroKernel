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
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-2}"
CAPTURE_HELPER="${ROOT_DIR}/scripts/capture_esp32_serial.py"
BUILD_LOG="/tmp/zerokernel_esp32_real_project_build.log"
SERIAL_LOG="/tmp/zerokernel_esp32_real_project_serial.log"
RESTORE_BUILD_LOG="/tmp/zerokernel_esp32_real_project_restore.log"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

restore_safe() {
  echo "Restoring KernelIdentity on ${PORT}"
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" --board-options UploadSpeed=115200 \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --upload -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" \
    2>&1 | tee "${RESTORE_BUILD_LOG}" >/dev/null || true
}

trap restore_safe EXIT

"${ARDUINO_CLI}" compile --fqbn "${FQBN}" --board-options UploadSpeed=115200 \
  --library "${ROOT_DIR}" \
  --upload -p "${PORT}" \
  "${ROOT_DIR}/examples/RealProjectNode" \
  2>&1 | tee "${BUILD_LOG}"

python3 "${CAPTURE_HELPER}" \
  --port "${PORT}" \
  --seconds "${CAPTURE_SECONDS}" \
  --start-delay "${CAPTURE_START_DELAY}" \
  --output "${SERIAL_LOG}"

grep 'REAL_PROJECT_NODE' "${SERIAL_LOG}" | tail -n 1
