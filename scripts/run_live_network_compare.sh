#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
BOARD="${1:-esp32}"
PORT_OVERRIDE="${2:-}"
ESP8266_NET_PROFILE="${ESP8266_NET_PROFILE:-mqtt}"
CAPTURE_HELPER="${ROOT_DIR}/scripts/capture_esp32_serial.py"
WORK_DIR="${ROOT_DIR}/scripts/.live-network"
HTTP_STATUS_FILE="${WORK_DIR}/http-status.json"
HTTP_LOG="${WORK_DIR}/http-server.log"
MQTT_LOG="${WORK_DIR}/mqtt-sub.log"
BASELINE_LOG="${WORK_DIR}/baseline-${BOARD}.log"
NODE_LOG="${WORK_DIR}/node-${BOARD}.log"
BASELINE_BUILD_LOG="${WORK_DIR}/baseline-${BOARD}-build.log"
NODE_BUILD_LOG="${WORK_DIR}/node-${BOARD}-build.log"
RESTORE_BUILD_LOG="${WORK_DIR}/restore-${BOARD}-build.log"
HTTP_PORT="${HTTP_PORT:-3000}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_TOPIC="${MQTT_TOPIC:-zerokernel/live/telemetry}"
SSID="${SSID:-sayaganteng}"
PASSWORD="${PASSWORD:-sayaganteng}"
HTTP_CAPTURE_SECONDS="${HTTP_CAPTURE_SECONDS:-14}"
CAPTURE_START_DELAY="${CAPTURE_START_DELAY:-2}"
MQTT_CAPTURE_COUNT="${MQTT_CAPTURE_COUNT:-50}"
HTTP_SERVER_PID=""
MQTT_SUB_PID=""

mkdir -p "${WORK_DIR}"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "arduino-cli not found at ${ARDUINO_CLI}" >&2
  exit 1
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required for local MQTT validation" >&2
  exit 1
fi

case "${BOARD}" in
  esp32)
    PORT="${PORT_OVERRIDE:-/dev/ttyUSB0}"
    FQBN="esp32:esp32:esp32"
    BOARD_OPTIONS=(--board-options UploadSpeed=115200)
    ;;
  esp8266|wemos)
    BOARD="esp8266"
    PORT="${PORT_OVERRIDE:-/dev/ttyUSB1}"
    FQBN="esp8266:esp8266:d1_mini"
    BOARD_OPTIONS=()
    ;;
  *)
    echo "Unsupported board: ${BOARD}" >&2
    exit 1
    ;;
esac

HOST_IP="$(ip route get 1.1.1.1 2>/dev/null | awk '/src/ {print $7; exit}')"
if [[ -z "${HOST_IP}" ]]; then
  HOST_IP="$(hostname -I | awk '{print $1}')"
fi
if [[ -z "${HOST_IP}" ]]; then
  echo "Unable to determine host IP for live network test" >&2
  exit 1
fi

write_secrets() {
  local target="$1"
  cat >"${target}" <<EOF
#ifndef ZEROKERNEL_GENERATED_LIVE_NETWORK_SECRETS_H
#define ZEROKERNEL_GENERATED_LIVE_NETWORK_SECRETS_H

static const char* kWiFiSsid = "${SSID}";
static const char* kWiFiPassword = "${PASSWORD}";
static const char* kHttpHost = "${HOST_IP}";
static const uint16_t kHttpPort = ${HTTP_PORT};
static const char* kHttpPath = "/api/data";
static const char* kMqttHost = "${HOST_IP}";
static const uint16_t kMqttPort = ${MQTT_PORT};
static const char* kMqttTopic = "${MQTT_TOPIC}";

#endif
EOF
}

cleanup() {
  if [[ -n "${MQTT_SUB_PID}" ]]; then
    kill "${MQTT_SUB_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${HTTP_SERVER_PID}" ]]; then
    kill "${HTTP_SERVER_PID}" >/dev/null 2>&1 || true
  fi
  docker rm -f zerokernel-mqtt-broker >/dev/null 2>&1 || true
}
trap cleanup EXIT

write_secrets "${ROOT_DIR}/examples/LiveNetworkBaseline/LocalSecrets.h"
write_secrets "${ROOT_DIR}/examples/LiveNetworkNode/LocalSecrets.h"

python3 "${ROOT_DIR}/scripts/live_http_receiver.py" \
  --host 0.0.0.0 \
  --port "${HTTP_PORT}" \
  --status-file "${HTTP_STATUS_FILE}" \
  >"${HTTP_LOG}" 2>&1 &
HTTP_SERVER_PID=$!
sleep 1

MQTT_CONFIG="${WORK_DIR}/mosquitto.conf"
cat >"${MQTT_CONFIG}" <<EOF
listener ${MQTT_PORT} 0.0.0.0
allow_anonymous true
persistence false
EOF

docker rm -f zerokernel-mqtt-broker >/dev/null 2>&1 || true
docker run -d --name zerokernel-mqtt-broker --network host \
  -v "${MQTT_CONFIG}:/mosquitto/config/mosquitto.conf:ro" \
  eclipse-mosquitto:2 >/dev/null
sleep 2

docker run --rm --network host eclipse-mosquitto:2 \
  mosquitto_sub -h 127.0.0.1 -p "${MQTT_PORT}" -t "${MQTT_TOPIC}" -v \
  >"${MQTT_LOG}" 2>&1 &
MQTT_SUB_PID=$!

compile_and_upload() {
  local sketch_path="$1"
  local build_log="$2"
  local extra_flags="-DZEROKERNEL_PROFILE_NETWORK_NODE"
  if [[ "${BOARD}" == "esp8266" && "${sketch_path}" == "${ROOT_DIR}/examples/LiveNetworkNode" ]]; then
    case "${ESP8266_NET_PROFILE}" in
      mqtt|"")
        ;;
      http)
        extra_flags="${extra_flags} -DZEROKERNEL_LIVE_ESP8266_HTTP_FIRST=1"
        ;;
      *)
        echo "Unsupported ESP8266 profile: ${ESP8266_NET_PROFILE}" >&2
        exit 1
        ;;
    esac
  fi
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" "${BOARD_OPTIONS[@]}" \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=${extra_flags}" \
    --upload -p "${PORT}" \
    "${sketch_path}" 2>&1 | tee "${build_log}"
}

restore_safe() {
  "${ARDUINO_CLI}" compile --fqbn "${FQBN}" "${BOARD_OPTIONS[@]}" \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE" \
    --upload -p "${PORT}" \
    "${ROOT_DIR}/examples/KernelIdentity" \
    >"${RESTORE_BUILD_LOG}" 2>&1 || true
}

capture_serial() {
  local output_file="$1"
  python3 "${CAPTURE_HELPER}" \
    --port "${PORT}" \
    --seconds "${HTTP_CAPTURE_SECONDS}" \
    --start-delay "${CAPTURE_START_DELAY}" \
    --output "${output_file}"
}

parse_last_line() {
  local anchor="$1"
  local path="$2"
  python3 - "$anchor" "$path" <<'PY'
import sys
anchor, path = sys.argv[1:3]
line = ""
with open(path, "r", encoding="utf-8", errors="ignore") as handle:
    for raw in handle:
        idx = raw.find(anchor)
        if idx != -1:
            line = raw[idx:].strip()
if line:
    print(line)
    raise SystemExit(0)
raise SystemExit(1)
PY
}

echo "Using host IP ${HOST_IP} for HTTP and MQTT"

echo "Running live baseline on ${BOARD} (${PORT})"
compile_and_upload "${ROOT_DIR}/examples/LiveNetworkBaseline" "${BASELINE_BUILD_LOG}"
capture_serial "${BASELINE_LOG}"
BASELINE_LINE="$(parse_last_line "LIVE_NET window_ms=" "${BASELINE_LOG}")"
echo "${BASELINE_LINE}"

echo "Running live module node on ${BOARD} (${PORT})"
compile_and_upload "${ROOT_DIR}/examples/LiveNetworkNode" "${NODE_BUILD_LOG}"
capture_serial "${NODE_LOG}"
NODE_LINE="$(parse_last_line "LIVE_NETMODULES window_ms=" "${NODE_LOG}")"
echo "${NODE_LINE}"

sleep 1
HTTP_STATUS="$(curl -m 3 -fsS "http://127.0.0.1:${HTTP_PORT}/api/status")"
MQTT_MESSAGES="$(wc -l < "${MQTT_LOG}" | tr -d ' ')"

restore_safe

python3 - "${BASELINE_LINE}" "${NODE_LINE}" "${BASELINE_BUILD_LOG}" "${NODE_BUILD_LOG}" "${HTTP_STATUS}" "${MQTT_MESSAGES}" <<'PY'
import json
import re
import sys

base_line, node_line, base_build_path, node_build_path, http_status_raw, mqtt_messages = sys.argv[1:7]

def parse_metrics(line):
    metrics = {}
    for key, value in re.findall(r"([a-z_]+)=([0-9]+)", line):
        metrics[key] = int(value)
    return metrics

def parse_resource(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    ram = re.search(
        r"Global variables use (\d+) bytes \((?:\d+)%\) of dynamic memory, leaving \d+ bytes for local variables. Maximum is (\d+) bytes.",
        text,
    )
    if not ram:
        ram = re.search(
            r"Variables and constants in RAM \(global, static\), used (\d+) / (\d+) bytes",
            text,
        )
    flash = re.search(
        r"Sketch uses (\d+) bytes \((?:\d+)%\) of program storage space. Maximum is (\d+) bytes.",
        text,
    )
    if not flash:
        flash = re.search(
            r"Code in flash .* used (\d+) / (\d+) bytes",
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

base = parse_metrics(base_line)
node = parse_metrics(node_line)
base_res = parse_resource(base_build_path)
node_res = parse_resource(node_build_path)
http_status = json.loads(http_status_raw)

print("---- live network compare ----")
print(base_line)
print(node_line)
print("ram_bytes:", describe(base_res["ram"], node_res["ram"]))
print("flash_bytes:", describe(base_res["flash"], node_res["flash"]))
print("sample_runs:", base.get("sample_runs", 0), "->", node.get("sample_runs", 0))
print("fast_avg_lag_us:", base.get("fast_avg_lag_us", 0), "->", node.get("fast_avg_lag_us", 0))
print("fast_max_lag_us:", base.get("fast_max_lag_us", 0), "->", node.get("fast_max_lag_us", 0))
print("fast_miss:", base.get("fast_miss", 0), "->", node.get("fast_miss", 0))
print("wifi_attempts:", base.get("wifi_attempts", 0), "->", node.get("wifi_attempts", 0))
print("wifi_reconnects:", base.get("wifi_reconnects", 0), "->", node.get("wifi_reconnects", 0))
print("http_ok:", base.get("http_ok", 0), "->", node.get("http_ok", 0))
print("http_fail:", base.get("http_fail", 0), "->", node.get("http_fail", 0))
print("http_rate:", base.get("http_rate", 0), "->", node.get("http_rate", 0))
print("mqtt_ok:", base.get("mqtt_ok", 0), "->", node.get("mqtt_ok", 0))
print("mqtt_fail:", base.get("mqtt_fail", 0), "->", node.get("mqtt_fail", 0))
print("mqtt_rate:", base.get("mqtt_rate", 0), "->", node.get("mqtt_rate", 0))
print("http_status_online:", int(bool(http_status.get("online"))))
print("http_status_requests:", int(http_status.get("requests", 0)))
print("mqtt_messages_captured:", int(mqtt_messages))
PY
