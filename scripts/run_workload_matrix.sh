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

EXAMPLES=(
  "EnvMonitorBaseline:POWER_SAVE"
  "EnvMonitorNode:POWER_SAVE"
  "TelemetryGatewayBaseline:LEAN_NET"
  "TelemetryGatewayNode:LEAN_NET"
  "IndustrialLoopBaseline:LEAN_NET"
  "IndustrialLoopNode:LEAN_NET"
)

TARGETS=(
  "esp8266:esp8266:d1_mini"
  "esp32:esp32:esp32"
  "rp2040:rp2040:rpipico"
  "STMicroelectronics:stm32:GenF4:pnum=BLACKPILL_F411CE"
)

profile_flag() {
  case "$1" in
    POWER_SAVE) echo "-DZEROKERNEL_PROFILE_POWER_SAVE" ;;
    TINY) echo "-DZEROKERNEL_PROFILE_TINY" ;;
    LEAN_NET) echo "-DZEROKERNEL_PROFILE_LEAN_NET" ;;
    *) echo "" ;;
  esac
}

for target in "${TARGETS[@]}"; do
  echo "== ${target} =="
  for entry in "${EXAMPLES[@]}"; do
    example="${entry%%:*}"
    profile="${entry##*:}"
    flag="$(profile_flag "${profile}")"
    build_log="/tmp/zerokernel_${example}_${target//[:=,]/_}.log"

    if "${ARDUINO_CLI}" compile --fqbn "${target}" \
      --library "${ROOT_DIR}" \
      --build-property "build.extra_flags=${flag}" \
      "${ROOT_DIR}/examples/${example}" \
      >"${build_log}" 2>&1; then
      ram="$(sed -n 's/.*Global variables use \([0-9][0-9]*\) bytes.*/\1/p' "${build_log}" | tail -n 1)"
      flash="$(sed -n 's/.*Sketch uses \([0-9][0-9]*\) bytes.*/\1/p' "${build_log}" | tail -n 1)"
      echo "PASS ${example} profile=${profile} ram=${ram:-n/a} flash=${flash:-n/a}"
    else
      echo "FAIL ${example} profile=${profile}"
      tail -n 20 "${build_log}" || true
      exit 1
    fi
  done
done
