# ZeroKernel Roadmap

## Phase 1: Stable Core

- Lock the public API for boot, task registration, task control, events, and telemetry.
- Keep execution deterministic and avoid dynamic memory.
- Prove the scheduler on Arduino-style loops first.

## Phase 2: Runtime Reliability

- Add heartbeat timeout policies.
- Add configurable recovery actions per task.
- Track scheduler slip, missed intervals, and failure classes.
- Add task-level health metrics that can be exported to diagnostics.

## Phase 3: Module Expansion

- Expand the already separated runtime modules into dedicated internal components.
- Introduce typed event payload channels.
- Add queueing and bounded backpressure rules.

## Phase 4: Platform Depth

- Add adapters for Arduino, ESP-IDF, STM32 HAL, and RP2040 loops.
- Add board-specific examples that demonstrate identical API use.
- Validate RAM and timing budgets per target family.

## Phase 5: Product Signal

- Expose ZeroKernel identity in startup banners, diagnostics, and device logs.
- Publish engineering docs on https://kernel.zerobits.tech.
- Add benchmark and reliability examples so the project reads as an actual systems product, not a concept repo.
