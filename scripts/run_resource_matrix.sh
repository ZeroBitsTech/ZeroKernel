#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="$(command -v arduino-cli 2>/dev/null || true)"
if [[ -z "${ARDUINO_CLI}" ]]; then
  ARDUINO_CLI="${ROOT_DIR}/bin/arduino-cli"
fi
ENFORCE_BUDGET=0

for arg in "$@"; do
  if [[ "${arg}" == "--enforce-budget" ]]; then
    ENFORCE_BUDGET=1
  fi
done

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
  local profile_name="$4"
  shift 4 || true

  local log_file
  log_file="$(mktemp)"

  echo "[compile] ${label}"
  "${ARDUINO_CLI}" compile --fqbn "${fqbn}" "$@" "${sketch}" 2>&1 | tee "${log_file}"

  if [[ "${ENFORCE_BUDGET}" -eq 1 ]]; then
    python3 - "${label}" "${profile_name}" "${log_file}" --enforce-budget <<'PY'
import re
import sys

label, profile_name, log_path = sys.argv[1], sys.argv[2], sys.argv[3]

with open(log_path, "r", encoding="utf-8", errors="ignore") as fh:
    text = fh.read()

ram = re.search(r"Global variables use (\d+) bytes .* Maximum is (\d+) bytes", text)
if not ram:
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)

flash = re.search(r"Sketch uses (\d+) bytes .* Maximum is (\d+) bytes", text)
if not flash:
    flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", text)

data = re.search(r"╠══ DATA\s+(\d+)", text)
rodata = re.search(r"╠══ RODATA\s+(\d+)", text)
bss = re.search(r"╚══ BSS\s+(\d+)", text)

def describe(match):
    if not match:
        return "n/a"
    used = int(match.group(1))
    total = int(match.group(2))
    percent = (used * 100.0) / total if total else 0.0
    return f"{used}/{total} ({percent:.1f}%)"

def scalar(match):
    if not match:
        return "n/a"
    return match.group(1)

budget_map = {
    "ESP8266 UniversalSmokeTest": {"ram": 30000, "flash": 245000},
    "ESP32 Sensor Hub": {"ram": 30000, "flash": 300000},
    "RP2040 Loop Monitor": {"ram": 15000, "flash": 70000},
    "STM32 Control Loop": {"ram": 6000, "flash": 20000},
}

def metric_used(match):
    if not match:
        return None
    return int(match.group(1))

budget = budget_map.get(label, {})
ram_used = metric_used(ram)
flash_used = metric_used(flash)
ram_ok = ram_used is None or ram_used <= budget.get("ram", ram_used)
flash_ok = flash_used is None or flash_used <= budget.get("flash", flash_used)
budget_ok = ram_ok and flash_ok

print(
    f"[resource] {label} profile={profile_name} "
    f"flash={describe(flash)} ram={describe(ram)} "
    f"data={scalar(data)} rodata={scalar(rodata)} bss={scalar(bss)}"
)
print(
    f"[budget] {label} "
    f"ram={'PASS' if ram_ok else 'FAIL'} "
    f"flash={'PASS' if flash_ok else 'FAIL'} "
    f"overall={'PASS' if budget_ok else 'FAIL'}"
)

if not budget_ok:
    raise SystemExit(2)
PY
  else
    python3 - "${label}" "${profile_name}" "${log_file}" <<'PY'
import re
import sys

label, profile_name, log_path = sys.argv[1], sys.argv[2], sys.argv[3]

with open(log_path, "r", encoding="utf-8", errors="ignore") as fh:
    text = fh.read()

ram = re.search(r"Global variables use (\d+) bytes .* Maximum is (\d+) bytes", text)
if not ram:
    ram = re.search(r"Variables and constants in RAM .* used (\d+) / (\d+) bytes", text)

flash = re.search(r"Sketch uses (\d+) bytes .* Maximum is (\d+) bytes", text)
if not flash:
    flash = re.search(r"Code in flash .* used (\d+) / (\d+) bytes", text)

data = re.search(r"╠══ DATA\s+(\d+)", text)
rodata = re.search(r"╠══ RODATA\s+(\d+)", text)
bss = re.search(r"╚══ BSS\s+(\d+)", text)

def describe(match):
    if not match:
        return "n/a"
    used = int(match.group(1))
    total = int(match.group(2))
    percent = (used * 100.0) / total if total else 0.0
    return f"{used}/{total} ({percent:.1f}%)"

def scalar(match):
    if not match:
        return "n/a"
    return match.group(1)

budget_map = {
    "ESP8266 UniversalSmokeTest": {"ram": 30000, "flash": 245000},
    "ESP32 Sensor Hub": {"ram": 30000, "flash": 300000},
    "RP2040 Loop Monitor": {"ram": 15000, "flash": 70000},
    "STM32 Control Loop": {"ram": 6000, "flash": 20000},
}

def metric_used(match):
    if not match:
        return None
    return int(match.group(1))

budget = budget_map.get(label, {})
ram_used = metric_used(ram)
flash_used = metric_used(flash)
ram_ok = ram_used is None or ram_used <= budget.get("ram", ram_used)
flash_ok = flash_used is None or flash_used <= budget.get("flash", flash_used)
budget_ok = ram_ok and flash_ok
enforce_budget = False

for arg in sys.argv[4:]:
    if arg == "--enforce-budget":
        enforce_budget = True

print(
    f"[resource] {label} profile={profile_name} "
    f"flash={describe(flash)} ram={describe(ram)} "
    f"data={scalar(data)} rodata={scalar(rodata)} bss={scalar(bss)}"
)
print(
    f"[budget] {label} "
    f"ram={'PASS' if ram_ok else 'FAIL'} "
    f"flash={'PASS' if flash_ok else 'FAIL'} "
    f"overall={'PASS' if budget_ok else 'FAIL'}"
)

if enforce_budget and not budget_ok:
    raise SystemExit(2)
PY
  fi

  rm -f "${log_file}"
}

if has_core "esp8266:esp8266"; then
  run_compile \
    "ESP8266 UniversalSmokeTest" \
    "esp8266:esp8266:d1_mini" \
    "${ROOT_DIR}/examples/UniversalSmokeTest" \
    "POWER_SAVE" \
    --library "${ROOT_DIR}" \
    --build-property "build.extra_flags=-DZEROKERNEL_PROFILE_POWER_SAVE"
fi

if has_core "esp32:esp32"; then
  run_compile \
    "ESP32 Sensor Hub" \
    "esp32:esp32:esp32" \
    "${ROOT_DIR}/examples/ESP32SensorHub" \
    "DEFAULT" \
    --library "${ROOT_DIR}"
fi

if has_core "rp2040:rp2040"; then
  run_compile \
    "RP2040 Loop Monitor" \
    "rp2040:rp2040:rpipico" \
    "${ROOT_DIR}/examples/RP2040LoopMonitor" \
    "DEFAULT" \
    --library "${ROOT_DIR}"
fi

if has_core "STMicroelectronics:stm32"; then
  run_compile \
    "STM32 Control Loop" \
    "STMicroelectronics:stm32:GenF4" \
    "${ROOT_DIR}/examples/STM32ControlLoop" \
    "DEFAULT" \
    --library "${ROOT_DIR}"
fi
