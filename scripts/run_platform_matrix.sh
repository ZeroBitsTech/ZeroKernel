#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

CORE_LIST="$("${ARDUINO_CLI}" core list)"

has_core() {
  local core_name="$1"
  grep -q "^${core_name}[[:space:]]" <<<"${CORE_LIST}"
}

run_compile() {
  local label="$1"
  local fqbn="$2"
  local sketch="$3"
  shift 3 || true

  echo "[compile] ${label}"
  "${ARDUINO_CLI}" compile --fqbn "${fqbn}" "$@" "${sketch}"
}

if has_core "esp8266:esp8266"; then
  run_compile \
    "ESP8266 UniversalSmokeTest" \
    "esp8266:esp8266:d1_mini" \
    "${ROOT_DIR}/examples/UniversalSmokeTest" \
    --library "${ROOT_DIR}"
else
  echo "[skip] esp8266:esp8266 core not installed"
fi

if has_core "esp32:esp32"; then
  run_compile \
    "ESP32 Sensor Hub" \
    "esp32:esp32:esp32" \
    "${ROOT_DIR}/examples/ESP32SensorHub" \
    --library "${ROOT_DIR}"
else
  echo "[skip] esp32:esp32 core not installed"
fi

if has_core "rp2040:rp2040"; then
  run_compile \
    "RP2040 Loop Monitor" \
    "rp2040:rp2040:rpipico" \
    "${ROOT_DIR}/examples/RP2040LoopMonitor" \
    --library "${ROOT_DIR}"
else
  echo "[skip] rp2040:rp2040 core not installed"
fi

if has_core "STMicroelectronics:stm32"; then
  run_compile \
    "STM32 Control Loop" \
    "STMicroelectronics:stm32:GenF4" \
    "${ROOT_DIR}/examples/STM32ControlLoop" \
    --library "${ROOT_DIR}"
else
  echo "[skip] STMicroelectronics:stm32 core not installed"
fi
