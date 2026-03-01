# Beta Modules

The following optional modules are currently marked **BETA**:

- `ZeroWiFiMaintainer`
- `ZeroHttpPump`
- `ZeroMqttPump`
- `ZeroTransportMetrics`
- `ZeroNetProfileEsp8266`

They are already useful and validated on desktop plus ESP32 smoke tests, but they are not yet considered fully field-proven across all transport stacks and all board families.

## Target maturity right now

- **ESP32:** the current network stack is stable enough for production-style evaluation and controlled deployments when you validate against your real server or broker.
- **ESP8266 / Wemos:** still BETA for full dual-transport use. The constrained MQTT-first preset now has repeatable live runs where MQTT delivery is real and timing beats the naive baseline, but the broader HTTP+MQTT path is still under active hardening.
- **Other targets:** compile-supported, but network helpers should still be treated as validation targets until they see the same live-network coverage.

Recommended default:

- For constrained ESP8266 boards, start with `ZeroNetProfileEsp8266`. It is the recommended MQTT-first constrained preset: HTTP stays degraded/off by default, offline queueing is rejected, idle MQTT churn is lower, and the preset uses a lighter recommended idle strategy plus staggered network task starts so the board stays more predictable without hand-tuning every interval.
- If your board is intentionally HTTP-first, use `ZeroNetProfileEsp8266Http` as a separate opt-in path. It keeps the constrained timing discipline but avoids pretending that one default cadence can serve HTTP-heavy and MQTT-heavy nodes equally well. Treat it as experimental until your own endpoint validation confirms live delivery.

## What BETA Means Here

- API shape is usable, but may still tighten as real-world patterns accumulate.
- Performance and footprint are measured and improving, but still under active tuning.
- ESP32 already has the strongest live-network validation. The BETA label mainly remains because the ESP8266 path is not yet clean enough to claim cross-board stability.
- The modules are suitable for evaluation, prototypes, and controlled deployments.
- For production, validate them against your actual transport stack and retry policy.

## Not BETA

The core runtime (`scheduler`, `task registry`, `events`, `watchdog`, `state`, `panic`, `timing`) is the stable part of the project and is not flagged as BETA.
