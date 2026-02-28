# Beta Modules

The following optional modules are currently marked **BETA**:

- `ZeroWiFiMaintainer`
- `ZeroHttpPump`
- `ZeroMqttPump`
- `ZeroTransportMetrics`

They are already useful and validated on desktop plus ESP32 smoke tests, but they are not yet considered fully field-proven across all transport stacks and all board families.

## What BETA Means Here

- API shape is usable, but may still tighten as real-world patterns accumulate.
- Performance and footprint are measured and improving, but still under active tuning.
- The modules are suitable for evaluation, prototypes, and controlled deployments.
- For production, validate them against your actual transport stack and retry policy.

## Not BETA

The core runtime (`scheduler`, `task registry`, `events`, `watchdog`, `state`, `panic`, `timing`) is the stable part of the project and is not flagged as BETA.
