#include "../ZeroKernel.h"

namespace zerokernel {

void Kernel::setIdleStrategy(uint8_t idleStrategy) {
  idleStrategy_ = idleStrategy;
}

uint8_t Kernel::getIdleStrategy() const {
  return idleStrategy_;
}

void Kernel::setCapabilities(CapabilityMask capabilities) {
  capabilityMask_ = capabilities;
}

Kernel::CapabilityMask Kernel::capabilities() const {
  return capabilityMask_;
}

void Kernel::enableCapabilities(CapabilityMask capabilities) {
  capabilityMask_ |= capabilities;
}

void Kernel::disableCapabilities(CapabilityMask capabilities) {
  capabilityMask_ &= ~capabilities;
}

void Kernel::setSafeModeCapabilities(CapabilityMask capabilities) {
  safeModeCapabilityMask_ = capabilities;
}

Kernel::CapabilityMask Kernel::safeModeCapabilities() const {
  return safeModeCapabilityMask_;
}

void Kernel::onStateChange(StateChangeHandler handler) {
  stateChangeHandler_ = handler;
}

uint8_t Kernel::state() const {
  return kernelState_;
}

void Kernel::setPanicHandler(PanicHandler handler) {
  panicHandler_ = handler;
}

Kernel::PanicInfo Kernel::getLastPanic() const {
  return lastPanic_;
}

void Kernel::triggerPanic(uint8_t reason,
                          const char* taskName,
                          unsigned long observedValue,
                          unsigned long budgetValue,
                          uint8_t mode) {
  lastPanic_ = PanicInfo();
  lastPanic_.reason = reason;
  lastPanic_.mode = mode;
  copyLabel_(lastPanic_.taskName, sizeof(lastPanic_.taskName), taskName);
  lastPanic_.observedValue = observedValue;
  lastPanic_.budgetValue = budgetValue;

  if (mode == kPanicEnterSafeMode) {
    safeMode_ = true;
    safeModePriorityFloor_ = kPriorityCritical;
  }

  setKernelState_(kStatePanic);

  if (panicHandler_ != NULL) {
    panicHandler_(lastPanic_);
  }

  if (mode == kPanicRebootCallback && hardwareWatchdogBridge_.feed != NULL) {
    hardwareWatchdogBridge_.feed();
  }
}

void Kernel::setWatchdogPolicy(const WatchdogPolicy& policy) {
  watchdogPolicy_ = policy;

  if (watchdogPolicy_.maxConsecutiveFailures == 0) {
    watchdogPolicy_.maxConsecutiveFailures = 1;
  }
}

Kernel::WatchdogPolicy Kernel::getWatchdogPolicy() const {
  return watchdogPolicy_;
}

void Kernel::setHardwareWatchdogBridge(const HardwareWatchdogBridge& bridge) {
  hardwareWatchdogBridge_ = bridge;
}

Kernel::HardwareWatchdogBridge Kernel::getHardwareWatchdogBridge() const {
  return hardwareWatchdogBridge_;
}

void Kernel::setSignalHandler(SignalHandler handler) {
#if ZEROKERNEL_ENABLE_SIGNAL_HOOK
  signalHandler_ = handler;
#else
  (void)handler;
#endif
}

void Kernel::resetKernelStats_() {
  kernelStats_ = KernelStats();
  totalTickDurationMs_ = 0;
  totalTaskDurationMs_ = 0;
  totalLagMs_ = 0;
  totalTickCycles_ = 0;
  totalTaskCycles_ = 0;
  maxTickDurationMs_ = 0;
  maxTaskDurationMs_ = 0;
  maxObservedLagMs_ = 0;
  maxTickCycles_ = 0;
  maxTaskCycles_ = 0;
}

void Kernel::resetRuntimeState_(TaskSlot& slot, TimeMs nowMs) {
  slot.lastRunAtMs = nowMs;
  slot.lastDurationMs = 0;
  slot.lastHeartbeatAtMs = nowMs;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
  slot.lastLagMs = 0;
#endif
  slot.consecutiveFailures = 0;
  slot.state = slot.startEnabled ? kTaskReady : kTaskSuspended;
}

void Kernel::initializeTaskSlot_(TaskSlot& slot,
                                 const TaskConfig& config,
                                 TimeMs nowMs) {
  slot = TaskSlot();
  slot.inUse = true;
  slot.startEnabled = config.startEnabled;
  slot.name = config.name;
  slot.callback = config.callback;
  slot.intervalMs = config.intervalMs;
  slot.maxRuntimeMs = config.maxRuntimeMs;
  slot.heartbeatTimeoutMs = config.heartbeatTimeoutMs;
  slot.priority = config.priority;
  slot.maxRuntimeUs = config.contract.maxRuntimeUs;
  slot.contractFlags = config.contract.flags;
  slot.failureBudget = config.contract.failureBudget;
  slot.panicMode = config.contract.panicMode;
  slot.safeModePriorityFloor = config.contract.safeModePriorityFloor;
  slot.requiredCapabilities = config.requiredCapabilities;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
  slot.lastLagMs = 0;
  slot.maxLagMs = 0;
  slot.deadlineMissCount = 0;
#endif
  resetRuntimeState_(slot, nowMs);
}

void Kernel::recordExecutionSuccess_(TaskSlot& slot,
                                     TimeMs beforeRun,
                                     TimeMs afterRun) {
  const TimeMs dueAtMs = slot.lastRunAtMs + slot.intervalMs;
  const TimeMs lagMs = beforeRun > dueAtMs ? (beforeRun - dueAtMs) : 0;
  const TimeMs nextRunAnchorMs = afterRun > beforeRun ? afterRun : beforeRun;

  slot.lastRunAtMs = nextRunAnchorMs;
  slot.lastDurationMs = afterRun - beforeRun;
  slot.lastHeartbeatAtMs = afterRun;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
  slot.lastLagMs = lagMs;
  if (lagMs > slot.maxLagMs) {
    slot.maxLagMs = lagMs;
  }
#endif
  if (lagMs > maxObservedLagMs_) {
    maxObservedLagMs_ = lagMs;
  }
  totalLagMs_ += lagMs;
  if (lagMs > 0) {
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
    ++slot.deadlineMissCount;
#endif
    ++kernelStats_.deadlineMisses;
    emitSignal_(kSignalDeadlineMiss, slot.name, static_cast<unsigned long>(lagMs));

    if ((slot.contractFlags & kContractAllowDegrade) != 0U && kernelState_ == kStateNormal) {
      setKernelState_(kStateDegraded);
    }
  }
  slot.consecutiveFailures = 0;
  ++slot.runCount;
  ++kernelStats_.taskExecutions;
  totalTaskDurationMs_ += slot.lastDurationMs;
  if (slot.lastDurationMs > maxTaskDurationMs_) {
    maxTaskDurationMs_ = slot.lastDurationMs;
  }
}

void Kernel::recordFailure_(TaskSlot& slot, bool heartbeatTimeout) {
  const uint8_t failureBudget =
      slot.failureBudget > 0 ? slot.failureBudget : watchdogPolicy_.maxConsecutiveFailures;

  ++slot.failureCount;
  ++slot.consecutiveFailures;
  slot.state = kTaskFaulted;
  ++kernelStats_.taskFailures;
  ++kernelStats_.watchdogTrips;
  emitSignal_(kSignalTaskFailure, slot.name, slot.failureCount);

  if ((slot.contractFlags & kContractAllowDegrade) != 0U) {
    setKernelState_(kStateDegraded);
  }

  if (slot.consecutiveFailures > failureBudget) {
    const uint8_t priorityFloor = slot.safeModePriorityFloor > 0
                                      ? slot.safeModePriorityFloor
                                      : static_cast<uint8_t>(kPriorityHigh);
    const uint8_t panicMode =
        slot.panicMode != 0 ? slot.panicMode : static_cast<uint8_t>(kPanicEnterSafeMode);

    if ((slot.contractFlags & kContractCritical) != 0U) {
      triggerPanic(kPanicTaskFailure,
                   slot.name,
                   slot.failureCount,
                   failureBudget,
                   panicMode);
      return;
    }

    enterSafeMode(priorityFloor);
  }

  if (heartbeatTimeout) {
    ++kernelStats_.heartbeatTimeouts;
    emitSignal_(kSignalHeartbeatTimeout, slot.name, slot.failureCount);
  }
}

void Kernel::recoverIfAllowed_(TaskSlot& slot, TimeMs nowMs) {
  if (!watchdogPolicy_.autoRecovery) {
    return;
  }

  const uint8_t failureBudget =
      slot.failureBudget > 0 ? slot.failureBudget : watchdogPolicy_.maxConsecutiveFailures;

  if (slot.consecutiveFailures > failureBudget) {
    return;
  }

  slot.lastRunAtMs = nowMs;
  slot.lastHeartbeatAtMs = nowMs;
  slot.lastDurationMs = 0;
  slot.state = kTaskReady;
  ++kernelStats_.taskRecoveries;

  if (kernelState_ != kStatePanic) {
    setKernelState_(kStateRecovery);
    if (!safeMode_) {
      setKernelState_(kStateNormal);
    }
  }
}

bool Kernel::detectHeartbeatTimeout_(TaskSlot& slot, TimeMs nowMs) {
  TimeMs heartbeatTimeoutMs = slot.heartbeatTimeoutMs;
  if (heartbeatTimeoutMs == 0) {
    heartbeatTimeoutMs = watchdogPolicy_.heartbeatTimeoutMs;
  }

  if (heartbeatTimeoutMs == 0 || slot.state != kTaskReady) {
    return false;
  }

  const TimeMs timeoutWindowMs = slot.intervalMs + heartbeatTimeoutMs;
  if ((nowMs - slot.lastHeartbeatAtMs) <= timeoutWindowMs) {
    return false;
  }

  recordFailure_(slot, true);

  if ((slot.contractFlags & kContractCritical) != 0U &&
      (slot.contractFlags & kContractAllowDegrade) == 0U) {
    const uint8_t panicMode =
        slot.panicMode != 0 ? slot.panicMode : static_cast<uint8_t>(kPanicEnterSafeMode);
    triggerPanic(kPanicHeartbeatTimeout,
                 slot.name,
                 nowMs - slot.lastHeartbeatAtMs,
                 timeoutWindowMs,
                 panicMode);
  }

  recoverIfAllowed_(slot, nowMs);
  return true;
}

bool Kernel::isTaskDue_(const TaskSlot& slot, TimeMs nowMs) const {
  if (!slot.inUse || slot.state != kTaskReady) {
    return false;
  }

  return (nowMs - slot.lastRunAtMs) >= slot.intervalMs;
}

void Kernel::serviceHardwareWatchdog_(bool afterTask) {
  if (hardwareWatchdogBridge_.feed == NULL) {
    return;
  }

  if (!afterTask && !hardwareWatchdogBridge_.feedOnTick) {
    return;
  }

  if (afterTask && !hardwareWatchdogBridge_.feedAfterTask) {
    return;
  }

  hardwareWatchdogBridge_.feed();
}

void Kernel::setKernelState_(uint8_t nextState) {
  if (kernelState_ == nextState) {
    return;
  }

  kernelState_ = nextState;

  if (stateChangeHandler_ != NULL) {
    stateChangeHandler_(kernelState_);
  }
}

bool Kernel::hasTaskCapabilities_(const TaskSlot& slot) const {
  if (!slot.inUse) {
    return false;
  }

  if (slot.requiredCapabilities == kCapNone) {
    return true;
  }

  CapabilityMask activeCapabilities = capabilityMask_;
  if (safeMode_) {
    activeCapabilities &= safeModeCapabilityMask_;
  }

  return (slot.requiredCapabilities & activeCapabilities) == slot.requiredCapabilities;
}

}  // namespace zerokernel
