# Changelog

## 1.4.1

- Clarified target maturity: optional network modules are now documented as stable enough for ESP32 deployments that validate against their real endpoint, while ESP8266 remains under active hardening.
- Added transport readiness gating for `ZeroMqttPump`, matching the existing HTTP-side link probe and reducing futile broker connect attempts while Wi-Fi is still down.
- Synced runtime metadata: runtime version is now `1.4.1` and ABI version is published consistently as `2` in the manifest.

## Unreleased

- Added `ZeroNetProfileEsp8266`, a constrained MQTT-first preset for Wemos / ESP8266.
- Added optional offline queue gating in `ZeroHttpPump` and `ZeroMqttPump` so constrained boards can refuse backlog when link or transport is down.
- Reduced scheduler contention in the ESP8266 preset by staggering network task start offsets and lowering idle network task cadence.
- Added generic compare workloads for `EnvMonitor`, `TelemetryGateway`, and `IndustrialLoop`.
- Added repeatable ESP32 compare runners plus a cross-target workload compile matrix.
- Improved telemetry gateway tuning so module throughput rises without queue buildup.
- Fixed command handler registration so lean profiles fail safely instead of link-breaking.
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
