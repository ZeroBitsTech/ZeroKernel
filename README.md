# ZeroKernel

Small Bits. Solid Systems.

ZeroKernel is the NanoKernel-style embedded orchestration engine built by ZeroBits for microcontrollers that need deterministic behavior without the weight of a full RTOS.

Official URL: https://kernel.zerobits.tech

## Identity

- Name: ZeroKernel
- Vendor: ZeroBits
- Tagline: Small Bits. Solid Systems.
- Runtime access: `ZeroKernel.identity()`

## MVP scope

This initial stage provides:

- a cooperative, interval-based task scheduler
- fixed-size task registration with no dynamic allocation
- a simple publish/subscribe event hub
- runtime stats for tasks and kernel health
- automatic recovery signaling when a task overruns its runtime budget
- built-in runtime identity metadata for logs, boot banners, and integrations
- configurable watchdog policy for heartbeat timeouts and recovery limits

## Project structure

```text
ZeroKernel/
  README.md
  TODO.md
  library.properties
  zerokernel.manifest.json
  src/
    ZeroKernel.h
    ZeroKernelConfig.h
    core/
      KernelRuntime.cpp
      KernelIdentity.cpp
      TaskRegistry.cpp
      EventHub.cpp
      Watchdog.cpp
      KernelUtils.cpp
    diagnostics/
      KernelDiagnostics.cpp
    internal/
      KernelArch.h
      KernelArchAsm.h
      KernelArchAsm.S
      KernelHash.h
      KernelHash.c
      KernelString.h
      KernelString.cpp
    adapters/
      ArduinoClock.h
      Esp8266Watchdog.h
      Esp32Clock.h
      Esp32Watchdog.h
      LoopAdapter.h
      RP2040Clock.h
      Stm32Clock.h
  examples/
    BasicScheduler/
      BasicScheduler.ino
    CommandQueue/
      CommandQueue.ino
    ESP32SensorHub/
      ESP32SensorHub.ino
    ESP8266WiFiNode/
      ESP8266WiFiNode.ino
    FastKernelPrimitives/
      FastKernelPrimitives.ino
    KernelIdentity/
      KernelIdentity.ino
    DesktopSimulation/
      DesktopSimulation.cpp
    RP2040LoopMonitor/
      RP2040LoopMonitor.ino
    ResilientNode/
      ResilientNode.ino
    STM32ControlLoop/
      STM32ControlLoop.ino
    TypedEvents/
      TypedEvents.ino
    UniversalSmokeTest/
      UniversalSmokeTest.ino
    WemosDiagnosticConsole/
      WemosDiagnosticConsole.ino
    WemosStressBaseline/
      WemosStressBaseline.ino
    WemosStressLevel2/
      WemosStressLevel2.ino
    WemosStressZeroKernel/
      WemosStressZeroKernel.ino
  docs/
    api.md
    architecture.md
    configuration.md
    resource-budget.md
    reliability.md
    roadmap.md
    targets.md
    testing.md
  scripts/
    build_desktop_sim.sh
    run_desktop_audit.sh
    run_desktop_benchmark.sh
    run_desktop_lean_smoke.sh
    run_desktop_tests.sh
    run_full_audit.sh
    run_platform_matrix.sh
    run_resource_matrix.sh
    run_dual_board_command_smoke.sh
    run_esp32_compare.sh
    run_wemos_compare.sh
    run_wemos_diagnostics.sh
    run_wemos_level2.sh
  tests/
    desktop/
      KernelTests.cpp
  benchmarks/
    desktop/
      KernelBenchmark.cpp
```

## Core API

```cpp
#include <ZeroKernel.h>

void setup() {
  ZeroKernel.begin(millis);
  ZeroKernel.addTask("SensorReader", readSensor, 500, 10);
}

void loop() {
  ZeroKernel.tick();
}
```

```cpp
const zerokernel::Kernel::Identity& identity = ZeroKernel.identity();
Serial.println(identity.name);
Serial.println(identity.vendor);
Serial.println(identity.tagline);
```

```cpp
zerokernel::Kernel::WatchdogPolicy policy = {250, 2, true};
ZeroKernel.setWatchdogPolicy(policy);
ZeroKernel.heartbeatTask("SensorReader");
ZeroKernel.setTaskHeartbeatTimeout("SensorReader", 300);
ZeroKernel.enterSafeMode();
if (ZeroKernel.isSafeMode()) {
  ZeroKernel.exitSafeMode();
}
ZeroKernel.publishDeferred("telemetry.temperature", 42);
ZeroKernel.enqueueCommand("control.refresh", 1);
const zerokernel::Kernel::TopicKey telemetryKey =
    zerokernel::Kernel::makeTopicKey("telemetry.temperature");
ZeroKernel.publishDeferredFast(telemetryKey, 42);
ZeroKernel.scheduleWorkTyped(
    flushTelemetry,
    zerokernel::Kernel::EventValue::fromUnsigned(42));
ZeroKernel.setFlags(0x01U);
if (ZeroKernel.takeFlags(0x01U) != 0U) {
  ZeroKernel.flushWork();
}
ZeroKernel.flushEvents();
ZeroKernel.flushCommands();
ZeroKernel.publishTyped("status.online",
                        zerokernel::Kernel::EventValue::fromBool(true));
ZeroKernel.unsubscribe("temperature");
```

## Current design choices

- Task capacity is fixed at 8 to keep RAM usage predictable.
- Event subscribers are fixed at 16.
- Task callbacks must be non-blocking.
- Timing can be driven by an injected clock source or by `tick(nowMs)` for non-Arduino targets.
- Engine identity strings are stored as static constants to avoid scheduler overhead.
- The core is split by concern so scheduler, task control, events, and watchdog logic can evolve independently.
- Per-task heartbeat timeout overrides can tighten or relax watchdog behavior without heap allocation.
- Event subscriptions can now be attached and removed cleanly at runtime.
- Deferred event delivery uses a fixed-size queue, so event bursts stay bounded and heap-free.
- Deferred events and deferred commands both coalesce duplicate queued payloads, reducing avoidable queue churn.
- Queue drain budgets are capped per tick, so one burst is less likely to monopolize the scheduler.
- Topic keys now let deferred event and command queues store compact numeric routing ids instead of full labels.
- Fast-path publish helpers avoid repeated label hashing once a topic key is cached at boot.
- Fast subscription and command registration APIs now allow key-first routing without paying per-slot copied label storage.
- `ZEROKERNEL_ENABLE_LEGACY_LABEL_API` now lets small builds strip most string-compatibility comparisons and drop label-pointer storage from subscriber and command tables while keeping the public API available.
- `ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY` now lets lean builds collapse string publish/enqueue helpers into direct key-routing wrappers.
- `ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS` now lets smaller profiles drop per-task lag bookkeeping while preserving kernel-level timing reports.
- `ZEROKERNEL_ENABLE_DIAGNOSTICS` now lets production profiles strip formatted dump helpers and their format strings from the core build.
- Cooperative event flags now provide a tiny synchronization primitive without adding heap use or blocking APIs.
- An optional work queue now provides deferred non-urgent processing outside hot task callbacks.
- Queue backpressure can now drop the oldest entry instead of stalling newer state updates when bursts fill the buffer.
- Desktop tests and benchmark scripts are included so behavior and baseline performance can be verified locally.
- Scheduler execution is priority-aware and exposes lag/deadline metrics for tighter control on constrained systems.
- Typed events now support `long`, `unsigned long`, `bool`, and pointer payloads without changing the heap-free runtime model.
- Runtime signal hooks can surface drops, deadline misses, overruns, and failures to external logging or telemetry.
- Compile-time config and trace buffering now make the same core easier to scale from tiny boards to larger multi-module firmware.
- Profile-based configuration presets make it easier to pick sane defaults for tiny, extended, or diagnostics-heavy firmware.
- Hardware watchdog bridge callbacks and dump APIs make runtime diagnostics and MCU watchdog integration much easier in real devices.
- Resource and hardware test scripts now make memory footprint and board-side stress behavior easier to compare and repeat.
- Wemos-specific diagnostics and full-audit workflows now make the ESP8266 path much easier to validate end-to-end on real hardware.
- A dedicated command queue now separates deferred commands from pub/sub events, reducing queue contention and making orchestration more predictable.
- Power-save adapters now expose a lower-level idle hint, including ARM `WFI` where available, without forcing asm into the portable core.
- Safe mode can now temporarily restrict execution to higher-priority tasks when the system needs a degraded but stable operating mode.
- ABI versioning and a machine-readable manifest now make runtime compatibility easier to validate for tools and AI agents.
- The kernel now exposes an explicit runtime state model and panic path instead of relying only on ad-hoc safe mode transitions.
- Task execution contracts can now declare criticality and runtime budget metadata for stronger supervision.
- Timing reports now expose worst-case and average scheduler/task timing directly from the runtime.
- On Xtensa targets, timing reports now also capture cycle-counter maxima using inline assembly in the internal arch layer.

## Reliability Guarantees

- No dynamic allocation in the active runtime path.
- Fixed-capacity scheduler, queue, and trace storage.
- Cooperative execution only: blocking task callbacks are considered invalid usage.
- Bounded queue drain per tick with explicit backpressure.
- Fault containment through watchdog supervision, recovery, safe mode, and panic routing.
- Architecture-specific low-level instructions stay in adapters/internal helpers, not in the portable public API.

## Next build steps

1. Add topic-key-only subscriptions for users who want to avoid label storage entirely.
2. Add deeper edge-case tests around timer wraparound and subscription churn.
3. Add optional board-native watchdog bridges beyond ESP8266 and ESP32.
4. Add safe-mode boot and persistent fault counters for boards with RTC or NVS support.
