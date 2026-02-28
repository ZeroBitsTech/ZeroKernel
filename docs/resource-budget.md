# ZeroKernel Resource Budget

These numbers are the latest measured footprints from `scripts/run_resource_matrix.sh` and Wemos hardware validation. Treat them as regression references and release gates, not absolute guarantees.

## Budget policy

- `TINY`: target RAM under `30 KB` and keep `fast_miss = 0` in timing-sensitive smoke tests.
- `POWER_SAVE`: target RAM under `30 KB`, keep diagnostics stripped, and keep deterministic compare runs at `0 lag / 0 misses`.
- `EXTENDED`: target RAM under `40 KB` on constrained WiFi MCUs.
- `DIAGNOSTIC`: target RAM under `50 KB`, allowed to trade footprint for trace and richer observability.
- Keep program-size regressions small and intentional, especially on tighter targets.

## Cross-target snapshots

- ESP8266 `UniversalSmokeTest` (`POWER_SAVE`): RAM `28864 / 80192`, `data=1496`, `rodata=976`, `bss=26392`
- ESP32 `ESP32SensorHub` (`DEFAULT`): RAM `24676 / 327680`
- RP2040 `RP2040LoopMonitor` (`DEFAULT`): RAM `11440 / 262144`
- STM32 `STM32ControlLoop` (`DEFAULT`): RAM `3732 / 131072`

## Wemos comparison

- Blocking baseline: RAM `28300 / 80192`
- ZeroKernel comparison sketch: RAM `29044 / 80192`
- Current runtime overhead vs baseline:
  - RAM `+744 bytes`
  - Determinism maintained: `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`
- `WemosDiagnosticConsole` sample reading: `free_heap=49280` after boot/runtime warm-up on the current board

## Reading the numbers

- The most expensive KPI for ZeroKernel is deterministic timing. A small RAM increase is acceptable if `fast_miss` stays `0`.
- `POWER_SAVE` now strips legacy label storage and extended per-task lag bookkeeping, so small MCU builds stay closer to a lean topic-key-first runtime.
- Cross-target headroom remains healthy on ESP32, RP2040, and STM32 even with the same runtime model.
- Wemos remains the tightest budget target, so it is the best canary for RAM regressions.

## Release checklist

- Run `scripts/run_desktop_tests.sh`
- Run `scripts/run_desktop_benchmark.sh`
- Run `scripts/run_resource_matrix.sh`
- Run `scripts/run_wemos_compare.sh /dev/ttyUSB0`
- Reject the batch if determinism regresses on Wemos (`fast_miss > 0`) unless the change is explicitly experimental
