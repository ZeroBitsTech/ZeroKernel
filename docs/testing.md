# ZeroKernel Testing

## Local validation

- `scripts/build_desktop_sim.sh`: builds and runs the desktop simulation.
- `scripts/run_desktop_tests.sh`: runs the desktop regression suite.
- `scripts/run_desktop_lean_smoke.sh`: builds a lean `POWER_SAVE` profile and verifies that stripped diagnostics still behave safely.
- `scripts/run_desktop_benchmark.sh`: runs the desktop micro-benchmark. Pass `--enforce-performance` to fail on a synthetic slowdown beyond the soft gate.
- `scripts/run_desktop_audit.sh`: builds the desktop suite with stricter compiler warnings enabled.

## Platform matrix

- `scripts/run_platform_matrix.sh`: compiles platform examples for any installed Arduino cores and skips missing targets cleanly.
- `scripts/run_resource_matrix.sh`: compiles the same platform examples and prints a cross-target resource summary plus budget PASS/FAIL lines. Pass `--enforce-budget` to fail the script on a budget regression.

This is intended to keep portability visible even when all boards are not physically connected.

## Wemos comparison

- `scripts/run_wemos_compare.sh [/dev/ttyUSB0]`
- `scripts/run_wemos_compare.sh [/dev/ttyUSB0] [--enforce-determinism]`

This workflow:

1. flashes the blocking baseline sketch
2. captures serial metrics
3. flashes the ZeroKernel sketch
4. captures serial metrics
5. prints the before/after comparison

The comparison script also prints baseline vs ZeroKernel runtime footprint details for the tested ESP8266 sketch pair, then restores `KernelIdentity` automatically. With `--enforce-determinism`, it fails if the ZeroKernel run reports any lag or missed fast-cycle deadlines.

## ESP32 comparison

- `scripts/run_esp32_compare.sh [/dev/ttyUSB1]`

This mirrors the Wemos comparison flow on ESP32:

1. flashes the blocking baseline sketch
2. captures serial metrics
3. flashes the ZeroKernel sketch
4. captures serial metrics
5. prints before/after timing plus runtime footprint deltas
6. restores `KernelIdentity` automatically

## Wemos diagnostics

- `scripts/run_wemos_diagnostics.sh [/dev/ttyUSB0]`

This flashes a lightweight diagnostics sketch that prints:

- runtime identity
- kernel uptime, state, and ABI version
- timing report summary
- task dump lines
- compile-time RAM usage

## Higher-intent demos

- `examples/ESP32TelemetryNode`
- `scripts/run_esp32_telemetry_demo.sh [/dev/ttyUSB1]`

This runs a more realistic ESP32 node with:

- WiFi maintenance and reconnect signalling
- capability-gated diagnostics
- deferred telemetry events
- periodic runtime summaries over serial

- `examples/ESP32TelemetryBaseline`
- `scripts/run_esp32_telemetry_compare.sh [/dev/ttyUSB1]`

This compares the richer ESP32 telemetry workload against a manual-loop baseline and reports:

- sample cadence lag
- missed fast-cycle deadlines
- RAM deltas
- activity counts per 5-second window

- `examples/FaultInjectionDemo`
- `scripts/run_fault_injection_demo.sh [/dev/ttyUSB0] [fqbn]`

This runs a fault-focused runtime demo with:

- deliberate overruns
- signal hook output
- safe-mode entry
- automatic return to normal mode

- `examples/FaultInjectionBaseline`
- `scripts/run_fault_injection_compare.sh [/dev/ttyUSB0] [fqbn]`

This compares the fault-oriented workload against a manual-loop baseline and reports:

- heartbeat cadence lag
- failure and recovery counts
- RAM deltas
- ending runtime state

## Cross-board command smoke

- `examples/CommandQueue`
- `scripts/run_dual_board_command_smoke.sh [/dev/ttyUSB0] [/dev/ttyUSB1]`

This example is intended as a low-risk smoke test on both ESP8266 and ESP32:

- command queue dispatch
- deferred typed event delivery
- periodic runtime stats over serial
- automatic restore to `KernelIdentity` at the end of the smoke test

## Level-2 stress

- `scripts/run_wemos_level2.sh [/dev/ttyUSB0]`

This scenario combines:

- queue flood pressure
- deliberate execution overruns
- runtime trace capture
- hardware watchdog feeding through the ESP8266 adapter

The script flashes the board, captures the latest stress summary, and reports trace volume plus key failure counters.
It now restores `KernelIdentity` automatically when the capture finishes.

## Full audit

- `scripts/run_full_audit.sh [/dev/ttyUSB0]`

This runs the desktop audit, lean profile smoke, cross-target compile checks, cross-target resource summary, and the available Wemos hardware checks in one pass.
