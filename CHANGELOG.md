# Changelog

## Unreleased

- Added GitHub Actions CI and tag-driven release workflow.
- Added wiki-ready documentation pages under `docs/wiki/`.
- Added `RealProjectNode` as a more realistic network workload example.
- Added fairer ESP32 module compare tooling and improved serial capture.
- Reduced optional network module footprint via lean transport metrics and smaller default queues.
- Improved `ZeroHttpPump` and `ZeroMqttPump` throughput by allowing bounded intra-tick progress.
- Added `ZEROKERNEL_PROFILE_LEAN_NET` for tighter network-oriented builds.
- Reduced network module state again by compacting transport metrics counters.
- Hardened config validation with compile-time capacity guards and conflicting-flag checks.
- Copied task names into internal storage to avoid dangling name pointers.
- Replaced Xtensa platform idle hint from a no-op with a real idle instruction.

## 1.3.x

- Added capability-aware runtime supervision.
- Added optional network modules (`ZeroWiFiMaintainer`, `ZeroHttpPump`, `ZeroMqttPump`, `ZeroTransportMetrics`).
- Added cross-target examples and hardware validation on ESP8266 and ESP32.
