# Validation

## Automated Gates

- Desktop tests: `scripts/run_desktop_tests.sh`
- Lean smoke: `scripts/run_desktop_lean_smoke.sh`
- Desktop benchmark: `scripts/run_desktop_benchmark.sh --enforce-performance`
- Resource matrix: `scripts/run_resource_matrix.sh --enforce-budget`

## Hardware Validation

Current active field validation is strongest on:

- ESP8266 (`Wemos D1 mini`) for compare, stress, and seismic-node validation
- ESP32 for module smoke tests, telemetry compare, and real-project network simulation

## Known Tooling Limits

- `arduino-cli` compile jobs should run sequentially. Parallel compile runs can collide in cache paths.
- ESP32 upload can occasionally fail mid-flash due to `esptool` transport instability; retrying the upload usually succeeds.
- Serial capture on ESP32 is valid but can truncate if capture starts mid-stream; the provided capture helper reduces this but does not eliminate it entirely.

## Reading Performance Results

- Favor deterministic metrics first: `fast_miss`, average lag, max lag.
- Compare `success rate` and queue depth, not just raw `ok/fail` counts, when failure injection is part of the workload.
- Treat synthetic fail-injection examples as throughput/stability checks, not as production success-rate claims.
