# ZeroKernel API

## Boot

- `ZeroKernel.begin(clockSource)`: starts the kernel and binds an optional clock provider.
- `ZeroKernel.tick()`: runs the scheduler using the injected clock source.
- `ZeroKernel.tick(nowMs)`: runs the scheduler with an explicit timestamp for simulation or non-Arduino loops.

## Tasks

- `addTask(name, callback, intervalMs, maxRuntimeMs, startEnabled)`: registers a fixed-slot task.
- `addTask(TaskConfig)`: registers a task with full execution contract metadata.
- `suspendTask(name)`: pauses a task without removing it.
- `resumeTask(name)`: re-enables a suspended task.
- `restartTask(name)`: resets runtime timing and places the task back in ready state.
- `heartbeatTask(name)`: refreshes the task heartbeat manually.
- `setTaskHeartbeatTimeout(name, heartbeatTimeoutMs)`: overrides the global watchdog timeout for one task.
- `setTaskPriority(name, priority)`: adjusts task scheduling priority at runtime.

`TaskConfig` can carry an `ExecutionContract`:

- `maxRuntimeUs`: declared runtime budget metadata for finer supervision.
- `flags`: bitmask of:
  - `kContractCritical`
  - `kContractDropIfLate`
  - `kContractAllowDegrade`
- `requiredCapabilities`: bitmask of runtime capabilities required before the task is eligible to run.

## Watchdog

- `setWatchdogPolicy(policy)`: configures the global watchdog baseline.
- `getWatchdogPolicy()`: returns the active watchdog policy.
- `setSignalHandler(handler)`: installs a lightweight runtime signal hook for diagnostics.
- `setHardwareWatchdogBridge(bridge)`: installs a callback bridge to feed an MCU hardware watchdog.
- `getHardwareWatchdogBridge()`: returns the current bridge settings.
- `setIdleStrategy(strategy)`: configures the runtime idle policy.
- `getIdleStrategy()`: returns the active idle policy.
- `setCapabilities(mask)`: replaces the currently enabled runtime capability mask.
- `capabilities()`: returns the active runtime capability mask.
- `enableCapabilities(mask)`: enables one or more runtime capabilities.
- `disableCapabilities(mask)`: disables one or more runtime capabilities.
- `setSafeModeCapabilities(mask)`: defines which capabilities remain active while safe mode is enabled.
- `safeModeCapabilities()`: returns the safe-mode capability mask.
- `onStateChange(handler)`: installs a callback for kernel state changes.
- `state()`: returns the active kernel state.
- `setPanicHandler(handler)`: installs a panic callback.
- `getLastPanic()`: returns the last panic snapshot.
- `triggerPanic(reason, taskName, observedValue, budgetValue, mode)`: triggers the panic path explicitly.
- `enterSafeMode(minimumPriority)`: restricts scheduling to higher-priority tasks.
- `exitSafeMode()`: returns the scheduler to normal operation.
- `isSafeMode()`: reports whether safe mode is active.
- `makeTopicKey(label)`: hashes a topic label once so repeated dispatch can use a cached numeric key.
- `abiVersion()`: returns the runtime ABI version.
- `runtimeVersion()`: returns the runtime semantic version.

`WatchdogPolicy` fields:

- `heartbeatTimeoutMs`: additional tolerance beyond the task interval.
- `maxConsecutiveFailures`: recovery limit before a task remains faulted.
- `autoRecovery`: enables automatic return to `kTaskReady`.

`KernelSignal` can emit:

- `kSignalTaskFailure`
- `kSignalHeartbeatTimeout`
- `kSignalDeadlineMiss`
- `kSignalEventDrop`
- `kSignalExecutionOverrun`
- `kSignalCommandDrop`
- `kSignalWorkDrop`

`KernelState` values:

- `kStateBoot`
- `kStateNormal`
- `kStateDegraded`
- `kStateSafeMode`
- `kStateRecovery`
- `kStatePanic`

`IdleStrategy` values:

- `kIdleBusy`
- `kIdleYield`
- `kIdleSleep`
- `kIdlePlatformHint`

`PanicMode` values:

- `kPanicUseDefault`
- `kPanicFreeze`
- `kPanicEnterSafeMode`
- `kPanicRebootCallback`

`Capability` values:

- `kCapNone`
- `kCapIO`
- `kCapNetwork`
- `kCapStorage`
- `kCapTelemetry`
- `kCapDiagnostics`
- `kCapRadio`
- `kCapControl`
- `kCapCustom0`
- `kCapAll`

## Events

- `subscribe(topic, handler)`: attaches a handler to a topic.
- `subscribeFast(topicKey, handler, topicLabel)`: attaches a handler directly to a cached key and can omit label storage.
- `subscribeTyped(topic, handler)`: attaches a typed handler that receives `EventValue`.
- `subscribeTypedFast(topicKey, handler, topicLabel)`: attaches a typed handler directly to a cached key.
- `unsubscribe(topic, handler)`: removes one matching handler, or all handlers on a topic if `handler == NULL`.
- `unsubscribeFast(topicKey, handler)`: removes one or all handlers registered by key.
- `unsubscribeTyped(topic, handler)`: removes typed subscriptions on a topic.
- `unsubscribeTypedFast(topicKey, handler)`: removes typed handlers registered by key.
- `publish(topic, value)`: sends a `long` payload to current subscribers.
- `publishTyped(topic, value)`: sends a typed payload directly to current typed subscribers.
- `publishFast(topicKey, value)`: sends a `long` payload using a cached topic key.
- `publishTypedFast(topicKey, value)`: sends a typed payload using a cached topic key.
- `publishDeferred(topic, value)`: queues a `long` payload into the bounded event ring buffer.
- `publishDeferredTyped(topic, value)`: queues a typed payload into the bounded event ring buffer.
- `publishDeferredFast(topicKey, value)`: queues a `long` payload using a cached topic key.
- `publishDeferredTypedFast(topicKey, value)`: queues a typed payload using a cached topic key.
- `flushEvents()`: drains the deferred event queue immediately.
- `registerCommand(command, handler)`: attaches a typed handler to a bounded command queue channel.
- `registerCommandFast(commandKey, handler, commandLabel)`: attaches a command handler directly to a cached key.
- `unregisterCommand(command, handler)`: removes one or all matching command handlers.
- `unregisterCommandFast(commandKey, handler)`: removes command handlers registered by key.
- `enqueueCommand(command, value)`: queues a `long` command payload.
- `enqueueCommandTyped(command, value)`: queues a typed command payload.
- `enqueueCommandFast(commandKey, value)`: queues a `long` command payload using a cached key.
- `enqueueCommandTypedFast(commandKey, value)`: queues a typed command payload using a cached key.
- `flushCommands()`: drains the command queue immediately.
- `scheduleWork(handler)`: queues a deferred work item with an empty payload.
- `scheduleWorkTyped(handler, value)`: queues a typed deferred work item.
- `flushWork()`: drains the work queue immediately.
- `setFlags(mask)`: sets one or more cooperative event flags.
- `clearFlags(mask)`: clears one or more cooperative event flags.
- `hasFlags(mask, requireAll)`: checks whether one or any flags are currently set.
- `takeFlags(mask)`: returns matching flags and clears them atomically within the cooperative loop.
- `subscriptionCount()`: returns active subscription slots.
- `typedSubscriptionCount()`: returns active typed subscription slots.
- `commandHandlerCount()`: returns active command handler slots.
- `queuedEventCount()`: returns the number of queued deferred events.
- `queuedCommandCount()`: returns the number of queued deferred commands.
- `queuedWorkCount()`: returns the number of queued deferred work items.

When `ZEROKERNEL_ENABLE_LEGACY_LABEL_API` is disabled, the string overloads remain available for compatibility but runtime dispatch falls back to cached keys and callback labels become empty strings unless a fast registration explicitly passes a label.

`EventValue` supports:

- `fromLong(value)`
- `fromUnsigned(value)`
- `fromBool(value)`
- `fromPointer(value)`

## Diagnostics

- `getTaskStats(name, outStats)`: fills task runtime data.
- `snapshotTasks(buffer, capacity)`: copies current task stats into a caller-provided buffer.
- `snapshotTrace(buffer, capacity)`: copies the newest trace entries into a caller-provided buffer.
- `traceCount()`: returns the number of stored trace entries.
- `clearTrace()`: clears the trace ring buffer.
- `dumpStats(writer)`: writes a formatted one-line kernel summary.
- `dumpTasks(writer)`: writes formatted task summaries.
- `dumpTrace(writer)`: writes formatted trace lines.
- `getStats()`: returns kernel-wide counters.
- `getTimingReport()`: returns worst-case and average scheduler/task timing data.
- `identity()`: returns the built-in engine identity for logs and boot banners.

When `ZEROKERNEL_ENABLE_DIAGNOSTICS` is disabled, the dump APIs remain link-safe but compile to no-op stubs.

Key counters exposed by `KernelStats`:

- `taskExecutions`
- `taskFailures`
- `taskRecoveries`
- `deadlineMisses`
- `eventsDelivered`
- `commandsQueued`
- `commandsDelivered`
- `workQueued`
- `workDelivered`
- `watchdogTrips`
- `heartbeatTimeouts`
- `executionOverruns`
- `queuedEventsDropped`
- `queuedCommandsDropped`
- `queuedWorkDropped`

`TaskStats.requiredCapabilities` reports the task capability mask exactly as registered.
