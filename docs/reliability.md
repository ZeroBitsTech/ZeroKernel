# ZeroKernel Reliability

## Reliability Guarantees

- No dynamic allocation in the active runtime path.
- Fixed-capacity scheduler, queue, and trace storage.
- Cooperative execution model: blocking task callbacks are out of contract.
- Bounded queue drain per tick with explicit backpressure behavior.
- Watchdog supervision with recovery, degraded state, safe mode, and panic routing.
- Runtime compatibility exposed through semantic version plus ABI version.

## Supervision Model

ZeroKernel now supervises execution in layers:

1. Task execution accounting
2. Watchdog failure detection
3. Kernel state transitions
4. Safe mode restriction
5. Panic routing

This keeps the runtime lightweight while still allowing deterministic failure handling.

## Failure Escalation

Typical escalation path:

1. Deadline miss or overrun is recorded
2. Task failure is counted
3. Kernel may enter `DEGRADED`
4. Kernel may enter `SAFE_MODE`
5. Critical faults may trigger `PANIC`

The exact path depends on watchdog policy and task execution contract flags.

## Portability Note

Low-level instructions and atomic helpers are kept in adapters/internal helpers so the public API and core control flow remain portable across:

- ESP8266
- ESP32
- RP2040
- STM32
- other Arduino-compatible MCU targets
