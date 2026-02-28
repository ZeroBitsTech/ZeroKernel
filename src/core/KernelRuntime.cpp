#include "../ZeroKernel.h"
#include "../internal/KernelArch.h"

namespace zerokernel {

Kernel::Kernel()
    : clockSource_(NULL),
      bootAtMs_(0),
      lastTickAtMs_(0),
      started_(false),
      hardwareWatchdogBridge_(),
      signalHandler_(NULL),
      stateChangeHandler_(NULL),
      panicHandler_(NULL),
      eventQueueHead_(0),
      eventQueueTail_(0),
      eventQueueCount_(0),
      commandQueueHead_(0),
      commandQueueTail_(0),
      commandQueueCount_(0),
      workQueueHead_(0),
      workQueueTail_(0),
      workQueueCount_(0),
      traceHead_(0),
      traceTail_(0),
      traceCount_(0),
      eventFlags_(0),
#if ZEROKERNEL_ENABLE_CAPABILITIES
      capabilityMask_(kCapAll),
      safeModeCapabilityMask_(kCapAll),
#endif
      registeredTaskCount_(0),
      readyTaskCount_(0),
      heartbeatMonitoringActive_(false),
      kernelState_(kStateBoot),
      safeMode_(false),
      safeModePriorityFloor_(kPriorityHigh),
      idleStrategy_(kIdlePlatformHint),
      lastPanic_(),
      totalTickDurationMs_(0),
      totalTaskDurationMs_(0),
      totalLagMs_(0),
      totalTickCycles_(0),
      totalTaskCycles_(0),
      maxTickDurationMs_(0),
      maxTaskDurationMs_(0),
      maxObservedLagMs_(0),
      maxTickCycles_(0),
      maxTaskCycles_(0) {
  for (uint8_t i = 0; i < kTaskStorage; ++i) {
    tasks_[i] = TaskSlot();
  }

  for (uint8_t i = 0; i < kSubscriberStorage; ++i) {
    subscribers_[i] = SubscriptionSlot();
  }

  for (uint8_t i = 0; i < kTypedSubscriberStorage; ++i) {
    typedSubscribers_[i] = TypedSubscriptionSlot();
  }

  for (uint8_t i = 0; i < kCommandHandlerStorage; ++i) {
    commandHandlers_[i] = CommandHandlerSlot();
  }

  resetEventQueue_();
  resetCommandQueue_();
  resetWorkQueue_();
  resetTrace_();

  resetKernelStats_();
  watchdogPolicy_.heartbeatTimeoutMs = 0;
  watchdogPolicy_.maxConsecutiveFailures = 3;
  watchdogPolicy_.autoRecovery = true;
  hardwareWatchdogBridge_.feed = NULL;
  hardwareWatchdogBridge_.feedOnTick = false;
  hardwareWatchdogBridge_.feedAfterTask = false;
}

void Kernel::begin(ClockSource clockSource) {
  clockSource_ = clockSource;
  bootAtMs_ = currentTime_();
  lastTickAtMs_ = bootAtMs_;
  started_ = true;

  resetKernelStats_();
  resetEventQueue_();
  resetCommandQueue_();
  resetWorkQueue_();
  resetTrace_();
  eventFlags_ = 0;
  lastPanic_ = PanicInfo();
  safeMode_ = false;
  safeModePriorityFloor_ = kPriorityHigh;
  idleStrategy_ = kIdlePlatformHint;
  setKernelState_(kStateNormal);
  syncTaskRuntimeHints_();
  kernelStats_.registeredTasks = registeredTaskCount_;
  kernelStats_.activeTasks = readyTaskCount_;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (!tasks_[i].inUse) {
      continue;
    }

    tasks_[i].runCount = 0;
    tasks_[i].failureCount = 0;
    tasks_[i].consecutiveFailures = 0;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
    tasks_[i].lastLagMs = 0;
    tasks_[i].maxLagMs = 0;
    tasks_[i].deadlineMissCount = 0;
#endif
    resetRuntimeState_(tasks_[i], bootAtMs_);
  }
}

void Kernel::tick() {
  tick(currentTime_());
}

void Kernel::tick(TimeMs nowMs) {
  const TimeMs tickBeginMs = nowMs;
  const uint32_t tickBeginCycles =
      internal::hasCycleCounter() ? internal::readCycleCounter() : 0U;
  if (!started_) {
    begin(clockSource_);

    if (clockSource_ == NULL) {
      bootAtMs_ = nowMs;
      lastTickAtMs_ = nowMs;

      for (uint8_t i = 0; i < kMaxTasks; ++i) {
        if (!tasks_[i].inUse) {
          continue;
        }

        tasks_[i].runCount = 0;
        tasks_[i].failureCount = 0;
        tasks_[i].consecutiveFailures = 0;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
        tasks_[i].lastLagMs = 0;
        tasks_[i].maxLagMs = 0;
        tasks_[i].deadlineMissCount = 0;
#endif
        resetRuntimeState_(tasks_[i], nowMs);
      }
    } else {
      nowMs = currentTime_();
    }
  }

  lastTickAtMs_ = nowMs;
  ++kernelStats_.schedulerLoops;
  serviceHardwareWatchdog_(false);
  if (heartbeatMonitoringActive_) {
    inspectHeartbeats_(nowMs);
  }

  const bool hasCycleCounter = internal::hasCycleCounter();

  TimeMs schedulerNowMs = nowMs;
  bool executedTask = false;

  while (true) {
    const int taskIndex = selectNextRunnableTask_(schedulerNowMs);
    if (taskIndex < 0) {
      break;
    }

    TaskSlot& slot = tasks_[taskIndex];
    const TimeMs beforeRun = schedulerNowMs;
    const uint32_t taskBeginCycles = hasCycleCounter ? internal::readCycleCounter() : 0U;
    slot.callback();

    TimeMs afterRun = beforeRun;
    if (clockSource_ != NULL) {
      afterRun = currentTime_();
    }

    if (taskBeginCycles != 0U) {
      const uint32_t taskEndCycles = internal::readCycleCounter();
      const uint32_t taskDeltaCycles = taskEndCycles - taskBeginCycles;
      totalTaskCycles_ += taskDeltaCycles;
      if (taskDeltaCycles > maxTaskCycles_) {
        maxTaskCycles_ = taskDeltaCycles;
      }
    }

    recordExecutionSuccess_(slot, beforeRun, afterRun);

    TimeMs runtimeBudgetMs = slot.maxRuntimeMs;
    if (slot.maxRuntimeUs > 0) {
      const TimeMs contractBudgetMs = static_cast<TimeMs>((slot.maxRuntimeUs + 999U) / 1000U);
      if (runtimeBudgetMs == 0 || contractBudgetMs < runtimeBudgetMs) {
        runtimeBudgetMs = contractBudgetMs;
      }
    }

    if (runtimeBudgetMs > 0 && slot.lastDurationMs > runtimeBudgetMs) {
      ++kernelStats_.executionOverruns;
      emitSignal_(kSignalExecutionOverrun,
                  slot.name,
                  static_cast<unsigned long>(slot.lastDurationMs));
      recordFailure_(slot, false);

      if ((slot.contractFlags & kContractCritical) != 0U &&
          (slot.contractFlags & kContractAllowDegrade) == 0U) {
        const uint8_t panicMode =
            slot.panicMode != 0 ? slot.panicMode : static_cast<uint8_t>(kPanicEnterSafeMode);
        triggerPanic(kPanicTaskOverrun,
                     slot.name,
                     slot.lastDurationMs,
                     runtimeBudgetMs,
                     panicMode);
      }

      recoverIfAllowed_(slot, afterRun);
    }

    drainDeferredQueues_();
    serviceHardwareWatchdog_(true);
    executedTask = true;

    if (clockSource_ != NULL) {
      schedulerNowMs = afterRun;
      if (heartbeatMonitoringActive_) {
        inspectHeartbeats_(schedulerNowMs);
      }
    }
  }

  if (!executedTask) {
    ++kernelStats_.schedulerIdleLoops;
  }

  drainDeferredQueues_();
  lastTickAtMs_ = schedulerNowMs;
  kernelStats_.uptimeMs = lastTickAtMs_ - bootAtMs_;
  kernelStats_.registeredTasks = registeredTaskCount_;
  kernelStats_.activeTasks = readyTaskCount_;

  TimeMs tickEndMs = schedulerNowMs;
  if (clockSource_ != NULL) {
    tickEndMs = currentTime_();
  }

  const TimeMs tickDurationMs = tickEndMs > tickBeginMs ? (tickEndMs - tickBeginMs) : 0;
  totalTickDurationMs_ += tickDurationMs;
  if (tickDurationMs > maxTickDurationMs_) {
    maxTickDurationMs_ = tickDurationMs;
  }

  if (tickBeginCycles != 0U) {
    const uint32_t tickEndCycles = internal::readCycleCounter();
    const uint32_t tickDeltaCycles = tickEndCycles - tickBeginCycles;
    totalTickCycles_ += tickDeltaCycles;
    if (tickDeltaCycles > maxTickCycles_) {
      maxTickCycles_ = tickDeltaCycles;
    }
  }
}

Kernel::KernelStats Kernel::getStats() const {
  KernelStats stats = kernelStats_;
  const TimeMs nowMs = clockSource_ != NULL ? currentTime_() : lastTickAtMs_;
  stats.uptimeMs = started_ ? (nowMs - bootAtMs_) : 0;
  stats.registeredTasks = registeredTaskCount_;
  stats.activeTasks = readyTaskCount_;
  return stats;
}

Kernel::TimingReport Kernel::getTimingReport() const {
  TimingReport report = {};
  const KernelStats stats = getStats();

  report.worstTickDurationMs = maxTickDurationMs_;
  report.averageTickDurationMs =
      stats.schedulerLoops == 0 ? 0 : static_cast<TimeMs>(totalTickDurationMs_ / stats.schedulerLoops);
  report.worstTaskDurationMs = maxTaskDurationMs_;
  report.averageTaskDurationMs =
      stats.taskExecutions == 0 ? 0 : static_cast<TimeMs>(totalTaskDurationMs_ / stats.taskExecutions);
  report.worstLagMs = maxObservedLagMs_;
  report.averageLagMs =
      stats.taskExecutions == 0 ? 0 : static_cast<TimeMs>(totalLagMs_ / stats.taskExecutions);
  report.worstTickCycles = maxTickCycles_;
  report.averageTickCycles = stats.schedulerLoops == 0
                                 ? 0
                                 : static_cast<uint32_t>(totalTickCycles_ / stats.schedulerLoops);
  report.worstTaskCycles = maxTaskCycles_;
  report.averageTaskCycles = stats.taskExecutions == 0
                                 ? 0
                                 : static_cast<uint32_t>(totalTaskCycles_ / stats.taskExecutions);
  report.totalTicks = stats.schedulerLoops;
  report.totalTaskRuns = stats.taskExecutions;
  report.deadlineMisses = stats.deadlineMisses;
  return report;
}

void Kernel::inspectHeartbeats_(TimeMs nowMs) {
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    TaskSlot& slot = tasks_[i];
    if (!slot.inUse || slot.state != kTaskReady) {
      continue;
    }

    detectHeartbeatTimeout_(slot, nowMs);
  }
}

void Kernel::drainDeferredQueues_() {
  if (eventQueueCount_ > 0) {
    drainEventQueue_(resolveDrainLimit_(
        eventQueueCount_,
        static_cast<uint8_t>(ZEROKERNEL_EVENT_DRAIN_PER_TICK),
        static_cast<uint8_t>(kMaxEventQueue)));
  }

  if (commandQueueCount_ > 0) {
    drainCommandQueue_(resolveDrainLimit_(
        commandQueueCount_,
        static_cast<uint8_t>(ZEROKERNEL_COMMAND_DRAIN_PER_TICK),
        static_cast<uint8_t>(kMaxCommandQueue)));
  }

  if (workQueueCount_ > 0) {
    drainWorkQueue_(resolveDrainLimit_(
        workQueueCount_,
        static_cast<uint8_t>(ZEROKERNEL_WORK_DRAIN_PER_TICK),
        static_cast<uint8_t>(kMaxWorkQueue)));
  }
}

void Kernel::syncTaskRuntimeHints_() {
  registeredTaskCount_ = 0;
  readyTaskCount_ = 0;
  heartbeatMonitoringActive_ = watchdogPolicy_.heartbeatTimeoutMs > 0;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    const TaskSlot& slot = tasks_[i];
    if (!slot.inUse) {
      continue;
    }

    ++registeredTaskCount_;
    if (slot.state == kTaskReady) {
      ++readyTaskCount_;
    }

    if (slot.heartbeatTimeoutMs > 0) {
      heartbeatMonitoringActive_ = true;
    }
  }
}

uint8_t Kernel::resolveDrainLimit_(uint8_t queuedCount,
                                   uint8_t baseLimit,
                                   uint8_t capacity) const {
  if (queuedCount == 0) {
    return 0;
  }

  if (baseLimit == 0) {
    return queuedCount;
  }

#if !ZEROKERNEL_ENABLE_ADAPTIVE_DRAIN
  return queuedCount < baseLimit ? queuedCount : baseLimit;
#else
  uint8_t limit = baseLimit;

  if (capacity > 0 && queuedCount > capacity / 2) {
    const uint8_t surge = static_cast<uint8_t>(queuedCount / 2);
    limit = static_cast<uint8_t>(baseLimit + surge);
  }

  if (capacity > 0 && queuedCount >= static_cast<uint8_t>((capacity * 3U) / 4U)) {
    limit = queuedCount;
  }

  return queuedCount < limit ? queuedCount : limit;
#endif
}

int Kernel::selectNextRunnableTask_(TimeMs nowMs) const {
  if (kernelState_ == kStatePanic && lastPanic_.mode == kPanicFreeze) {
    return -1;
  }

  int selectedIndex = -1;
  TimeMs selectedDueAtMs = 0;
  uint8_t selectedPriority = 0;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    const TaskSlot& slot = tasks_[i];
    if (!isTaskDue_(slot, nowMs)) {
      continue;
    }

    if (!hasTaskCapabilities_(slot)) {
      continue;
    }

    if (safeMode_ && slot.priority < safeModePriorityFloor_) {
      continue;
    }

    const TimeMs dueAtMs = slot.lastRunAtMs + slot.intervalMs;

    if (selectedIndex < 0 || slot.priority > selectedPriority ||
        (slot.priority == selectedPriority && dueAtMs < selectedDueAtMs)) {
      selectedIndex = static_cast<int>(i);
      selectedPriority = slot.priority;
      selectedDueAtMs = dueAtMs;
    }
  }

  return selectedIndex;
}

}  // namespace zerokernel

zerokernel::Kernel ZeroKernel;
