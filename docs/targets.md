# ZeroKernel Target Strategy

## Compatibility stance

ZeroKernel is written in standard C++ with no runtime heap dependency in its hot path. That keeps the core portable across:

- Arduino AVR boards
- ESP32 and ESP8266
- STM32 families
- RP2040
- nRF52
- bare-metal C++ loops
- desktop simulation builds for validation

## What makes it portable

- The core only depends on fixed-size arrays and function pointers.
- Time can come from `begin(clockSource)` or from explicit `tick(nowMs)`.
- The adapter layer is header-only and optional.
- No board-specific register access exists in the kernel core.

## What still needs target-specific work

- HAL drivers remain the responsibility of the consuming firmware.
- Serial/log sinks are external to the kernel.
- Board-specific watchdog hardware integration is not implemented yet.
- Interrupt-safe queues are not implemented yet.

## Current adapters

- `src/adapters/ArduinoClock.h`
- `src/adapters/Esp8266Watchdog.h`
- `src/adapters/Esp32Clock.h`
- `src/adapters/Esp32Watchdog.h`
- `src/adapters/LoopAdapter.h`
- `src/adapters/RP2040Clock.h`
- `src/adapters/Stm32Clock.h`

These adapters are intentionally small. The kernel stays independent, while integration helpers stay easy to replace per platform.

## Cross-target examples

- `examples/UniversalSmokeTest`
- `examples/ESP8266WiFiNode`
- `examples/ESP32SensorHub`
- `examples/RP2040LoopMonitor`
- `examples/STM32ControlLoop`
