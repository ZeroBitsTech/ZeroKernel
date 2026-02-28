# ZeroKernel Architecture

## Positioning

ZeroKernel is the NanoKernel-style runtime developed by ZeroBits for small embedded systems that still need deterministic orchestration and fault containment.

Core identity:

- Name: ZeroKernel
- Vendor: ZeroBits
- Tagline: Small Bits. Solid Systems.
- Site: https://kernel.zerobits.tech

This identity is exported in the runtime API through `ZeroKernel.identity()`, so devices, boot logs, dashboards, and integration layers can display the engine name without hardcoding brand strings in every project.

## Runtime layers

```text
Application Tasks
  -> Service Modules
  -> ZeroKernel Core
  -> HAL / Board Runtime
  -> Microcontroller
```

## Current core modules

- `Kernel`: public facade for boot, scheduling, telemetry, and control.
- `src/core/TaskRegistry.cpp`: task registration, lifecycle control, and stats access.
- `src/core/EventHub.cpp`: fixed-size classic/typed pub-sub plus a bounded deferred event queue.
- `src/core/Watchdog.cpp`: heartbeat policy, per-task timeout overrides, and recovery rules.
- `src/core/Watchdog.cpp`: heartbeat policy, per-task timeout overrides, recovery rules, and hardware watchdog bridge callbacks.
- `src/core/KernelIdentity.cpp`: static runtime brand identity exported to consuming firmware.
- `src/diagnostics/KernelDiagnostics.cpp`: optional debug dump formatting for stats, tasks, and trace output.
- `src/internal/KernelString.*`: bounded string helpers used by the runtime.
- `src/internal/KernelArch.h`: architecture-level intrinsics for idle hints and low-cost atomic helpers.
- `src/adapters/*.h`: optional integration helpers that keep board glue out of the core.
- The scheduler selects the highest-priority due task first and breaks ties by earliest due time.
- Runtime traces are stored in a fixed-size ring buffer for post-failure diagnostics.
- The runtime now exposes explicit kernel states (`BOOT`, `NORMAL`, `DEGRADED`, `SAFE_MODE`, `RECOVERY`, `PANIC`).
- Execution contracts let tasks declare criticality and supervision hints without requiring a full RTOS policy engine.

## Performance stance

- No dynamic allocation in the active scheduler path.
- Fixed capacities for task slots and subscribers.
- Fixed-capacity deferred event queue.
- Fixed-capacity work queue for deferred non-urgent callbacks.
- Typed event payloads stored inline in event messages, still without dynamic allocation.
- Function-pointer callbacks to avoid abstraction overhead.
- Optional injected clock source for platform portability.
- Branding metadata stored as compile-time constants, not rebuilt at runtime.
- Watchdog policy is global and fixed-size to avoid per-task heap or lookup overhead.
- Per-task heartbeat override is stored inline in each task slot to keep lookup cost constant.
- Compile-time macros allow capacity tuning without changing the public API.
- Low-level CPU instructions stay behind adapters/internal helpers so the core remains portable across MCU families.

## Near-term modularization

The core is now split by responsibility, but the external API still stays behind `ZeroKernel.h`. The next step is to deepen each area without destabilizing the public contract:

- `TaskRegistry`
- `EventHub`
- `Watchdog`
- `Telemetry`
- `PlatformAdapters`
