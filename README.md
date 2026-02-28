# ZeroKernel

Small Bits. Solid Systems.

ZeroKernel is the embedded execution runtime built by ZeroBits for microcontrollers that need deterministic scheduling, bounded memory, and fault-aware orchestration without the weight of a full RTOS.

Official URL: https://kernel.zerobits.tech

## Why ZeroKernel

ZeroKernel is designed for firmware that has outgrown ad-hoc `loop()` code but does not need a preemptive RTOS.

- Deterministic cooperative scheduling
- Fixed-capacity runtime with no dynamic allocation in the active path
- Key-based event and command routing for lean builds
- Watchdog, safe mode, panic routing, execution contracts, and capability masks
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
- Runtime identity, ABI version, manifest, timing reports, diagnostics hooks, and task-scoped capability gating
- Low-level internal helpers for cycle counting and idle hints, including C and assembly where it actually helps

## Current Runtime Snapshot

Latest measured references:

- ESP8266 `POWER_SAVE` (`UniversalSmokeTest`): `28864 / 80192` RAM
- Wemos compare runtime overhead vs blocking baseline: `+744 bytes` RAM
- Wemos determinism gate: `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`
- ESP32 compare runtime overhead vs blocking baseline: `+704 bytes` RAM
- ESP32 determinism gate: `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`
- Wemos measured free heap during diagnostics: `49280`

These are regression references, not marketing numbers. The most important gate is still deterministic timing.

## Before vs After

Measured on Wemos D1 mini using the blocking baseline and the current ZeroKernel compare sketch:

| Metric | Before (blocking) | After (ZeroKernel) |
| --- | ---: | ---: |
| RAM usage | `28300 / 80192` | `29044 / 80192` |
| Fast avg lag | `2512 us` | `0 us` |
| Fast max lag | `11054 us` | `0 us` |
| Fast misses | `126` | `0` |

Tradeoff summary:

- RAM overhead: `+744 bytes`
- Determinism maintained: `0 lag`, `0 misses`
- Measured free heap on Wemos diagnostics: `49280`

That overhead buys bounded scheduling, watchdog supervision, panic flow, capability-aware task gating, and fixed-capacity queues. On ESP8266-class hardware the measured free heap remains comfortably high, so the extra runtime footprint is a small and controlled tradeoff rather than a practical memory risk.

## ESP32 Before vs After

Measured on ESP32 using the blocking baseline and the current ZeroKernel compare sketch:

| Metric | Before (blocking) | After (ZeroKernel) |
| --- | ---: | ---: |
| RAM usage | `22116 / 327680` | `22820 / 327680` |
| Fast avg lag | `2022 us` | `0 us` |
| Fast max lag | `8156 us` | `0 us` |
| Fast misses | `124` | `0` |

Tradeoff summary:

- RAM overhead: `+704 bytes`
- Determinism maintained: `0 lag`, `0 misses`

The ESP32 tradeoff is also healthy: the footprint increase is small relative to total headroom, while the scheduling result is materially better under the same workload.

## Field Validation: ESP8266 Seismic Node

Measured on a real `ESP8266` direct-AP seismic node using:

- NodeMCU / ESP8266
- MPU6050-class accelerometer over I2C
- local buzzer alarm output
- direct access point mode with queued HTTP delivery
- live local backend over Wi-Fi AP during the test window

The original firmware was a single blocking loop. The ZeroKernel rewrite split the workload into sampling, heartbeat, flush, buzzer, temperature, and status tasks.

| Metric | Before (direct loop) | After (ZeroKernel, tuned) |
| --- | ---: | ---: |
| Sample runs (5s window) | `476` | `501` |
| Fast avg lag | `5393 us` | `6 us` |
| Fast max lag | `21733 us` | `2378 us` |
| Fast misses | `406` | `1` |
| Successful local sends | `5` | `7` |

Tradeoff summary:

- The direct-loop version remains functional, but it drifts and drops timing quality once live HTTP delivery is active.
- The tuned ZeroKernel version keeps the same node online, preserves successful local delivery, and holds sensor timing far closer to the target schedule.
- This is the kind of workload where the difference is visible on real hardware under real transport load, not only in a synthetic lab loop.

Real sample window from the direct AP seismic project:

```text
BASELINE_SEISMIC window_ms=5009 sample_runs=476 fast_avg_lag_us=5393 fast_max_lag_us=21733 fast_miss=406 queue_max=1 sent_ok=5 sent_fail=0 captures=5 clients=1
ZEROKERNEL_SEISMIC window_ms=5000 sample_runs=501 fast_avg_lag_us=6 fast_max_lag_us=2378 fast_miss=1 queue_max=3 sent_ok=7 sent_fail=0 captures=7 clients=1
```

Representative baseline vs ZeroKernel shape from the seismic firmware:

Baseline-style direct loop:

```cpp
void loop() {
  const unsigned long nowUs = micros();
  trackSampleTiming(nowUs);
  ++sampleRuns;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 1.0f;
  readAccel(ax, ay, az);
  updateMotionModel(ax, ay, az);

  if (shouldSend && canCapture) {
    capturePacket(true);
  } else if (heartbeatDue() && hasDirectClient()) {
    capturePacket(false);
  }

  if (txCount > 0) {
    flushQueueOnce();
  }

  delay(LOOP_DELAY_MS);
}
```

ZeroKernel rewrite:

```cpp
void sampleSensorTask() {
  trackSampleTiming(micros());
  ++sampleRuns;

  float ax = 0.0f;
  float ay = 0.0f;
  float az = 1.0f;
  readAccel(ax, ay, az);
  updateMotionModel(ax, ay, az);

  if (shouldSend && canCapture) {
    capturePacket(true);
  }
}

void setup() {
  ZeroKernel.begin(zerokernel::adapters::arduinoMillisClock);

  ZeroKernel.addTask("Sample", sampleSensorTask, LOOP_DELAY_MS, 4);
  ZeroKernel.addTask("Flush", queueFlushTask, 120, 3);
  ZeroKernel.addTask("Heartbeat", heartbeatTask, 25, 2);
  ZeroKernel.addTask(buzzerTaskConfig);
  ZeroKernel.addTask(tempTask);
  ZeroKernel.addTask(statusTask);

  ZeroKernel.setTaskPriority("Sample", zerokernel::Kernel::kPriorityCritical);
  ZeroKernel.setTaskPriority("Flush", zerokernel::Kernel::kPriorityLow);
}

void loop() {
  ZeroKernel.tick();
}
```

That rewrite is the practical difference: the same node behavior is split into bounded, phase-aligned tasks instead of one blocking loop that mixes sensing, transport, alarms, and status work together.

## ESP32 Telemetry Parity

The richer ESP32 telemetry workload is also now phase-aligned after the runtime scheduling fix:

- Baseline: `sample_runs=21`, `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`
- ZeroKernel: `sample_runs=21`, `fast_avg_lag_us=0`, `fast_max_lag_us=0`, `fast_miss=0`

This matters because the fix is global to periodic task scheduling. It is not a demo-only patch; it improves sensor, telemetry, heartbeat, and transport polling loops across supported targets.

## Install

### Arduino IDE

1. Download or clone this repository.
2. Put the `ZeroKernel` folder into your Arduino libraries directory:
   - Linux: `~/Arduino/libraries/`
   - Windows: `Documents/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`
3. Restart Arduino IDE.
4. Open an example from `File -> Examples -> ZeroKernel`.

### PlatformIO

Add the local library path in `platformio.ini`:

```ini
[env:your_board]
platform = espressif8266
board = d1_mini
framework = arduino
lib_deps =
  symlink:///absolute/path/to/ZeroKernel
```

Or vendor the repository inside your project and point `lib_extra_dirs` to it.

## How To Use

### 1. Include the header

```cpp
#include <ZeroKernel.h>
```

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

### 8. Gate tasks by capability when you need subsystem control

```cpp
zerokernel::Kernel::TaskConfig wifiTask = {
    "WiFiNode",
    pollWifi,
    100,
    0,
    0,
    zerokernel::Kernel::kPriorityHigh,
    true,
    {},
    zerokernel::Kernel::kCapNetwork | zerokernel::Kernel::kCapTelemetry};

ZeroKernel.addTask(wifiTask);
ZeroKernel.disableCapabilities(zerokernel::Kernel::kCapNetwork);
// WiFiNode will stay registered but will not be scheduled until the capability is re-enabled.
```

### Minimal full sketch

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

## Higher-Intent Demo Projects

- `examples/ESP32TelemetryNode`:
  a richer ESP32 node example with WiFi maintenance, capability-gated diagnostics, heartbeat events, and periodic runtime summaries.
- `examples/ESP32TelemetryBaseline`:
  a manual-loop baseline for the same ESP32 telemetry workload so timing overhead can be compared fairly.
- `examples/FaultInjectionDemo`:
  a fault-focused demo that injects overruns, exposes watchdog signals, enters safe mode, and then returns to normal operation.
- `examples/FaultInjectionBaseline`:
  a manual-loop baseline for the same fault-focused workload.

Quick runners:

- `scripts/run_esp32_telemetry_demo.sh /dev/ttyUSB1`
- `scripts/run_esp32_telemetry_compare.sh /dev/ttyUSB1`
- `scripts/run_fault_injection_demo.sh /dev/ttyUSB0`
- `scripts/run_fault_injection_compare.sh /dev/ttyUSB0`

## Validation Pipeline

ZeroKernel already includes local and hardware validation:

- Desktop regression suite
- Lean profile smoke test
- Desktop benchmark with optional performance gate
- Cross-target compile and resource matrix with budget gates
- Wemos before/after compare with determinism gate
- ESP32 before/after compare with determinism gate
- Wemos diagnostics and level-2 stress with automatic firmware restore

Main scripts:

- `scripts/run_desktop_tests.sh`
- `scripts/run_desktop_lean_smoke.sh`
- `scripts/run_desktop_benchmark.sh --enforce-performance`
- `scripts/run_resource_matrix.sh --enforce-budget`
- `scripts/run_wemos_compare.sh /dev/ttyUSB0 --enforce-determinism`
- `scripts/run_esp32_compare.sh /dev/ttyUSB1`
- `scripts/run_full_audit.sh /dev/ttyUSB0`

## Build Profiles

The runtime can be tuned at compile time through [ZeroKernelConfig.h](/home/pinszzii/Projects/ZeroKernel/src/ZeroKernelConfig.h) and project-level macros.

Key profiles:

- `ZEROKERNEL_PROFILE_TINY`
- `ZEROKERNEL_PROFILE_MINIMAL_RUNTIME`
- `ZEROKERNEL_PROFILE_POWER_SAVE`
- `ZEROKERNEL_PROFILE_NETWORK_NODE`
- `ZEROKERNEL_PROFILE_EXTENDED`
- `ZEROKERNEL_PROFILE_DIAGNOSTIC`

Important lean-build switches:

- `ZEROKERNEL_ENABLE_LEGACY_LABEL_API`
- `ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY`
- `ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS`
- `ZEROKERNEL_ENABLE_DIAGNOSTICS`
- `ZEROKERNEL_ENABLE_CAPABILITIES`

`ZEROKERNEL_ENABLE_CAPABILITIES` stays enabled in full profiles and is compiled out in `POWER_SAVE` and `MINIMAL_RUNTIME`, so lean targets do not pay extra static RAM for capability state.

`ZEROKERNEL_PROFILE_NETWORK_NODE` biases the runtime toward WiFi/BLE/MQTT-style firmware: key-first routing, bounded command/work queues, stronger drain budgets, and leaner metadata by default.
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
