# ZeroKernel Wiki

ZeroKernel is an embedded execution runtime for deterministic microcontroller firmware.

## Start Here

- Read the main [README](../../README.md) for installation, positioning, and before/after metrics.
- Read [API](../api.md) for the public runtime surface.
- Read [Architecture](../architecture.md) for the core design.
- Read [Validation](./Validation.md) for the current test strategy and hardware notes.
- Read [Beta Modules](./Beta-Modules.md) before adopting the optional network modules in production.

## Current Focus

- Keep the core runtime lean and deterministic.
- Treat optional network helpers as add-on modules with bounded, measurable cost.
- Preserve cross-target portability across ESP8266, ESP32, RP2040, and STM32-class boards.
