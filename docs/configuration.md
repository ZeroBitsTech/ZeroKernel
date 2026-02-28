# ZeroKernel Configuration

ZeroKernel can be tuned at compile time by defining configuration macros before including `ZeroKernel.h`, or by editing `src/ZeroKernelConfig.h`.

## Preset profiles

ZeroKernel also supports preset profiles that change the default capacities and feature flags without changing the API:

- `ZEROKERNEL_PROFILE_TINY`
- `ZEROKERNEL_PROFILE_MINIMAL_RUNTIME`
- `ZEROKERNEL_PROFILE_POWER_SAVE`
- `ZEROKERNEL_PROFILE_EXTENDED`
- `ZEROKERNEL_PROFILE_DIAGNOSTIC`

Explicit capacity or feature macros still win over a profile if both are defined.

## Capacity macros

- `ZEROKERNEL_MAX_TASKS`
- `ZEROKERNEL_MAX_SUBSCRIBERS`
- `ZEROKERNEL_MAX_TYPED_SUBSCRIBERS`
- `ZEROKERNEL_MAX_EVENT_QUEUE`
- `ZEROKERNEL_MAX_COMMAND_HANDLERS`
- `ZEROKERNEL_MAX_COMMAND_QUEUE`
- `ZEROKERNEL_MAX_WORK_QUEUE`
- `ZEROKERNEL_MAX_TRACE_ENTRIES`

These values define fixed storage sizes. Increase them for larger firmware orchestration graphs; decrease them for tighter RAM budgets.

## Feature macros

- `ZEROKERNEL_ENABLE_TYPED_EVENTS`
- `ZEROKERNEL_ENABLE_EVENT_COALESCING`
- `ZEROKERNEL_ENABLE_COMMAND_QUEUE`
- `ZEROKERNEL_ENABLE_WORK_QUEUE`
- `ZEROKERNEL_ENABLE_LEGACY_LABEL_API`
- `ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY`
- `ZEROKERNEL_ENABLE_TRACE`
- `ZEROKERNEL_ENABLE_DIAGNOSTICS`
- `ZEROKERNEL_ENABLE_DEBUG_DUMP`
- `ZEROKERNEL_ENABLE_ADAPTIVE_DRAIN`
- `ZEROKERNEL_ENABLE_SIGNAL_HOOK`
- `ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS`

When `ZEROKERNEL_ENABLE_LEGACY_LABEL_API` is disabled, string-compatibility comparisons are removed and subscriber or command slots stop storing label pointers in the runtime tables.
When `ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY` is enabled, string publish and queue helpers collapse into cached-key routing wrappers instead of keeping the legacy string dispatch path.
When `ZEROKERNEL_ENABLE_DIAGNOSTICS` is disabled, formatted dump helpers remain link-safe but compile down to no-op stubs and keep their format strings out of lean builds.
When `ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS` is disabled, task lag and per-task deadline counters stop consuming per-slot storage, but kernel-level timing reports remain available.

## Drain policy macros

- `ZEROKERNEL_EVENT_DRAIN_PER_TICK`
- `ZEROKERNEL_COMMAND_DRAIN_PER_TICK`
- `ZEROKERNEL_WORK_DRAIN_PER_TICK`

## Backpressure macros

- `ZEROKERNEL_EVENT_QUEUE_BACKPRESSURE`
- `ZEROKERNEL_COMMAND_QUEUE_BACKPRESSURE`
- `ZEROKERNEL_WORK_QUEUE_BACKPRESSURE`

Set these to:

- `ZEROKERNEL_QUEUE_DROP_NEWEST`
- `ZEROKERNEL_QUEUE_DROP_OLDEST`

These cap how many queued events, commands, and work items are drained per scheduler pass. Lower values reduce long burst stalls; higher values reduce queue latency.

These flags keep the public API stable while allowing selected runtime features to be disabled in smaller builds.

## Example

```cpp
#define ZEROKERNEL_MAX_TASKS 4
#define ZEROKERNEL_MAX_EVENT_QUEUE 8
#define ZEROKERNEL_MAX_COMMAND_QUEUE 8
#define ZEROKERNEL_MAX_TRACE_ENTRIES 8
#include <ZeroKernel.h>
```

```cpp
#define ZEROKERNEL_PROFILE_DIAGNOSTIC
#include <ZeroKernel.h>
```

The profile presets are intended for fast target bring-up:

- `TINY`: lower fixed storage and disabled signal hook by default
- `MINIMAL_RUNTIME`: smallest runtime profile with typed events, trace, and command/work queues disabled
- `POWER_SAVE`: tuned for smaller always-on nodes with reduced diagnostics and lower queue overhead
- `EXTENDED`: more task and subscriber capacity for larger orchestrations
- `DIAGNOSTIC`: larger queue and trace buffers for bring-up, soak tests, and field debugging

For cross-project consistency, prefer setting these macros in the consuming firmware build configuration rather than editing the library per target.
