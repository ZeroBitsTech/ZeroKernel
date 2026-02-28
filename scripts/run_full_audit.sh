#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-/dev/ttyUSB0}"

echo "[audit] desktop warnings + tests"
bash "${ROOT_DIR}/scripts/run_desktop_audit.sh"

echo "[audit] desktop lean profile smoke"
bash "${ROOT_DIR}/scripts/run_desktop_lean_smoke.sh"

echo "[audit] desktop benchmark gate"
bash "${ROOT_DIR}/scripts/run_desktop_benchmark.sh" --enforce-performance

echo "[audit] platform compile matrix"
bash "${ROOT_DIR}/scripts/run_platform_matrix.sh"

echo "[audit] platform resource matrix"
bash "${ROOT_DIR}/scripts/run_resource_matrix.sh" --enforce-budget

if [[ -e "${PORT}" ]]; then
  echo "[audit] wemos diagnostics"
  bash "${ROOT_DIR}/scripts/run_wemos_diagnostics.sh" "${PORT}"

  echo "[audit] wemos before/after compare"
  bash "${ROOT_DIR}/scripts/run_wemos_compare.sh" "${PORT}" --enforce-determinism

  echo "[audit] wemos level-2 stress"
  bash "${ROOT_DIR}/scripts/run_wemos_level2.sh" "${PORT}"
else
  echo "[skip] serial port ${PORT} not found"
fi
