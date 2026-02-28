# ZeroKernel

Small Bits. Solid Systems.

ZeroKernel is the embedded execution runtime built by ZeroBits for microcontrollers that need deterministic scheduling, bounded memory, and fault-aware orchestration without the weight of a full RTOS.

Official URL: https://kernel.zerobits.tech

## Why ZeroKernel

ZeroKernel is designed for firmware that has outgrown ad-hoc `loop()` code but does not need a preemptive RTOS.

- Deterministic cooperative scheduling
- Fixed-capacity runtime with no dynamic allocation in the active path
- Key-based event and command routing for lean builds
- Watchdog, safe mode, panic routing, and execution contracts
- Build profiles that scale from small always-on nodes to diagnostics-heavy bring-up

The intended position is simple:

- stronger than a basic scheduler library
- smaller and easier to reason about than a full RTOS
- practical for production firmware on ESP8266, ESP32, RP2040, STM32, and similar targets

## What It Provides

- Cooperative interval scheduler with priority-aware task selection
- Fixed-size task registry and bounded queues
- Pub/sub events, command queue, work queue, and cooperative event flags
- Watchdog supervision with heartbeat timeout, overrun handling, recovery, and safe mode
- Runtime identity, ABI version, manifest, timing reports, and diagnostics hooks
- Low-level internal helpers for cycle counting and idle hints, including C and assembly where it actually helps

## Current Runtime Snapshot

Latest measured references:

- ESP8266 `POWER_SAVE` (`UniversalSmokeTest`): `28864 / 80192` RAM, `241140 / 1048576` flash
- Wemos compare runtime overhead vs blocking baseline: `+744 bytes` RAM, `+3344 bytes` flash
- Wemos determinism gate: `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`
- Wemos measured free heap during diagnostics: `49280`

These are regression references, not marketing numbers. The most important gate is still deterministic timing.

## Before vs After

Measured on Wemos D1 mini using the blocking baseline and the current ZeroKernel compare sketch:

| Metric | Before (blocking) | After (ZeroKernel) |
| --- | ---: | ---: |
| RAM usage | `28300 / 80192` | `29044 / 80192` |
| Flash usage | `237092 / 1048576` | `240436 / 1048576` |
| Fast avg lag | `2916 us` | `0 us` |
| Fast max lag | `12112 us` | `0 us` |
| Fast misses | `126` | `0` |

Tradeoff summary:

- RAM overhead: `+744 bytes`
- Flash overhead: `+3344 bytes`
- Determinism maintained: `0 lag`, `0 misses`
- Measured free heap on Wemos diagnostics: `49280`

## Quick Start

```cpp
#include <ZeroKernel.h>

void readSensor() {
  // Non-blocking work only.
}

void setup() {
  ZeroKernel.begin(millis);
  ZeroKernel.addTask("SensorReader", readSensor, 500, 10);
}

void loop() {
  ZeroKernel.tick();
}
```

Key-based routing is available when you want the lean path:

```cpp
const zerokernel::Kernel::TopicKey telemetryKey =
    zerokernel::Kernel::makeTopicKey("telemetry.temperature");

ZeroKernel.publishDeferredFast(telemetryKey, 42);
```

## How To Use

### 1. Add the library

Put ZeroKernel in your Arduino libraries folder or include it directly in your firmware project.

### 2. Start the runtime

```cpp
ZeroKernel.begin(millis);
```

For desktop or custom targets, pass your own clock source:

```cpp
unsigned long boardClock() {
  return millis();
}

ZeroKernel.begin(boardClock);
```

### 3. Register non-blocking tasks

```cpp
void sampleSensors() {
  // Non-blocking work only.
}

void flushTelemetry() {
  // Keep this short and cooperative.
}

ZeroKernel.addTask("Sensors", sampleSensors, 100, 5);
ZeroKernel.addTask("Telemetry", flushTelemetry, 500, 10);
```

### 4. Tick the kernel inside `loop()`

```cpp
void loop() {
  ZeroKernel.tick();
}
```

### 5. Use events or fast topic keys

String route:

```cpp
ZeroKernel.publishDeferred("telemetry.temperature", 42);
```

Lean key route:

```cpp
const zerokernel::Kernel::TopicKey temperatureKey =
    zerokernel::Kernel::makeTopicKey("telemetry.temperature");

ZeroKernel.publishDeferredFast(temperatureKey, 42);
```

### 6. Add watchdog and recovery policy

```cpp
zerokernel::Kernel::WatchdogPolicy policy = {250, 2, true};
ZeroKernel.setWatchdogPolicy(policy);
ZeroKernel.setTaskHeartbeatTimeout("Sensors", 300);
ZeroKernel.heartbeatTask("Sensors");
```

### 7. Inspect runtime state

```cpp
const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
const zerokernel::Kernel::TimingReport timing = ZeroKernel.getTimingReport();

if (ZeroKernel.isSafeMode()) {
  ZeroKernel.exitSafeMode();
}
```

If diagnostics are enabled:

```cpp
ZeroKernel.dumpStats(printLine);
ZeroKernel.dumpTasks(printLine);
ZeroKernel.dumpTrace(printLine);
```

## Validation Pipeline

ZeroKernel already includes local and hardware validation:

- Desktop regression suite
- Lean profile smoke test
- Desktop benchmark with optional performance gate
- Cross-target compile and resource matrix with budget gates
- Wemos before/after compare with determinism gate
- Wemos diagnostics and level-2 stress with automatic firmware restore

Main scripts:

- `scripts/run_desktop_tests.sh`
- `scripts/run_desktop_lean_smoke.sh`
- `scripts/run_desktop_benchmark.sh --enforce-performance`
- `scripts/run_resource_matrix.sh --enforce-budget`
- `scripts/run_wemos_compare.sh /dev/ttyUSB0 --enforce-determinism`
- `scripts/run_full_audit.sh /dev/ttyUSB0`

## Build Profiles

The runtime can be tuned at compile time through [ZeroKernelConfig.h](/home/pinszzii/Projects/ZeroKernel/src/ZeroKernelConfig.h) and project-level macros.

Key profiles:

- `ZEROKERNEL_PROFILE_TINY`
- `ZEROKERNEL_PROFILE_MINIMAL_RUNTIME`
- `ZEROKERNEL_PROFILE_POWER_SAVE`
- `ZEROKERNEL_PROFILE_EXTENDED`
- `ZEROKERNEL_PROFILE_DIAGNOSTIC`

Important lean-build switches:

- `ZEROKERNEL_ENABLE_LEGACY_LABEL_API`
- `ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY`
- `ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS`
- `ZEROKERNEL_ENABLE_DIAGNOSTICS`
- `ZEROKERNEL_ENABLE_DEBUG_DUMP`

For small builds, the goal is to preserve the public API while collapsing runtime cost toward key-based routing and stripped diagnostics.

## Reliability Model

ZeroKernel is built around explicit runtime constraints:

- No dynamic allocation in the active runtime path
- Fixed-capacity scheduler, queues, and trace buffers
- Non-blocking task callbacks only
- Bounded drain and backpressure policies
- Fault containment through watchdog supervision, recovery, safe mode, and panic routing
- Architecture-specific low-level instructions remain isolated to internal or adapter layers

## Open Source Direction

ZeroKernel is structured to work well as an open-core infrastructure project:

- Open source: kernel core, scheduler, runtime, adapters, validation tooling
- Private / future commercial layers: higher-level deployment, cloud diagnostics, analytics, and enterprise integrations

The repository is now set up as a serious infrastructure codebase, not an Arduino toy project.

## Project Layout

```text
ZeroKernel/
  src/
    core/
    diagnostics/
    internal/
    adapters/
  examples/
  tests/
  benchmarks/
  docs/
  scripts/
```

Key files:

- [ZeroKernel.h](/home/pinszzii/Projects/ZeroKernel/src/ZeroKernel.h)
- [ZeroKernelConfig.h](/home/pinszzii/Projects/ZeroKernel/src/ZeroKernelConfig.h)
- [architecture.md](/home/pinszzii/Projects/ZeroKernel/docs/architecture.md)
- [api.md](/home/pinszzii/Projects/ZeroKernel/docs/api.md)
- [resource-budget.md](/home/pinszzii/Projects/ZeroKernel/docs/resource-budget.md)
- [testing.md](/home/pinszzii/Projects/ZeroKernel/docs/testing.md)

## Next Priorities

1. Keep pushing lean key-only routing so compatibility wrappers stay thin.
2. Continue shaving static RAM without sacrificing deterministic timing.
3. Tighten regression gates so size and performance drift is caught automatically.
4. Expand target-native watchdog bridges and recovery policy depth where it is worth the complexity.
