#include "../ZeroKernel.h"
#include "../internal/KernelString.h"

namespace zerokernel {

bool Kernel::addTask(const char* name,
                     TaskCallback callback,
                     TimeMs intervalMs,
                     TimeMs maxRuntimeMs,
                     bool startEnabled) {
  TaskConfig config = {
      name,
      callback,
      intervalMs,
      maxRuntimeMs,
      0,
      kPriorityNormal,
      startEnabled,
      ExecutionContract(),
      kCapNone};
  return addTask(config);
}

bool Kernel::addTask(const TaskConfig& config) {
  if (config.name == NULL || config.callback == NULL || config.intervalMs == 0) {
    return false;
  }

  if (findTaskIndex_(config.name) >= 0) {
    return false;
  }

  const int freeIndex = findFreeTaskIndex_();
  if (freeIndex < 0) {
    return false;
  }

  initializeTaskSlot_(tasks_[freeIndex], config, started_ ? currentTime_() : 0);
  kernelStats_.registeredTasks = taskCount();
  kernelStats_.activeTasks = activeTaskCount();
  return true;
}

bool Kernel::suspendTask(const char* name) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  tasks_[index].state = kTaskSuspended;
  kernelStats_.activeTasks = activeTaskCount();
  return true;
}

bool Kernel::resumeTask(const char* name) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  TaskSlot& slot = tasks_[index];
  resetRuntimeState_(slot, currentTime_());
  slot.state = kTaskReady;
  kernelStats_.activeTasks = activeTaskCount();
  return true;
}

bool Kernel::restartTask(const char* name) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  TaskSlot& slot = tasks_[index];
  resetRuntimeState_(slot, currentTime_());
  slot.state = kTaskReady;
  kernelStats_.activeTasks = activeTaskCount();
  return true;
}

bool Kernel::heartbeatTask(const char* name) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  tasks_[index].lastHeartbeatAtMs = currentTime_();
  return true;
}

bool Kernel::setTaskHeartbeatTimeout(const char* name, TimeMs heartbeatTimeoutMs) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  tasks_[index].heartbeatTimeoutMs = heartbeatTimeoutMs;
  return true;
}

bool Kernel::setTaskPriority(const char* name, uint8_t priority) {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  tasks_[index].priority = priority;
  return true;
}

void Kernel::enterSafeMode(uint8_t minimumPriority) {
  safeMode_ = true;
  safeModePriorityFloor_ = minimumPriority;
  setKernelState_(kStateSafeMode);
}

void Kernel::exitSafeMode() {
  safeMode_ = false;
  safeModePriorityFloor_ = kPriorityHigh;
  if (kernelState_ != kStatePanic) {
    setKernelState_(kStateNormal);
  }
}

bool Kernel::isSafeMode() const {
  return safeMode_;
}

bool Kernel::getTaskStats(const char* name, TaskStats& outStats) const {
  const int index = findTaskIndex_(name);
  if (index < 0) {
    return false;
  }

  const TaskSlot& slot = tasks_[index];
  outStats = TaskStats();
  copyLabel_(outStats.name, sizeof(outStats.name), labelOrEmpty_(slot.name));
  outStats.intervalMs = slot.intervalMs;
  outStats.maxRuntimeMs = slot.maxRuntimeMs;
  outStats.heartbeatTimeoutMs = slot.heartbeatTimeoutMs;
  outStats.priority = slot.priority;
  outStats.lastRunAtMs = slot.lastRunAtMs;
  outStats.lastDurationMs = slot.lastDurationMs;
  outStats.lastHeartbeatAtMs = slot.lastHeartbeatAtMs;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
  outStats.lastLagMs = slot.lastLagMs;
  outStats.maxLagMs = slot.maxLagMs;
#else
  outStats.lastLagMs = 0;
  outStats.maxLagMs = 0;
#endif
  outStats.runCount = slot.runCount;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
  outStats.deadlineMissCount = slot.deadlineMissCount;
#else
  outStats.deadlineMissCount = 0;
#endif
  outStats.failureCount = slot.failureCount;
  outStats.consecutiveFailures = slot.consecutiveFailures;
  outStats.maxRuntimeUs = slot.maxRuntimeUs;
  outStats.contractFlags = slot.contractFlags;
  outStats.failureBudget = slot.failureBudget;
  outStats.panicMode = slot.panicMode;
  outStats.safeModePriorityFloor = slot.safeModePriorityFloor;
#if ZEROKERNEL_ENABLE_CAPABILITIES
  outStats.requiredCapabilities = slot.requiredCapabilities;
#else
  outStats.requiredCapabilities = kCapNone;
#endif
  outStats.state = slot.state;
  return true;
}

uint8_t Kernel::snapshotTasks(TaskStats* buffer, uint8_t capacity) const {
  if (buffer == NULL || capacity == 0) {
    return 0;
  }

  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxTasks && count < capacity; ++i) {
    if (!tasks_[i].inUse) {
      continue;
    }

    TaskStats& outStats = buffer[count];
    outStats = TaskStats();
    copyLabel_(outStats.name, sizeof(outStats.name), labelOrEmpty_(tasks_[i].name));
    outStats.intervalMs = tasks_[i].intervalMs;
    outStats.maxRuntimeMs = tasks_[i].maxRuntimeMs;
    outStats.heartbeatTimeoutMs = tasks_[i].heartbeatTimeoutMs;
    outStats.priority = tasks_[i].priority;
    outStats.lastRunAtMs = tasks_[i].lastRunAtMs;
    outStats.lastDurationMs = tasks_[i].lastDurationMs;
    outStats.lastHeartbeatAtMs = tasks_[i].lastHeartbeatAtMs;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
    outStats.lastLagMs = tasks_[i].lastLagMs;
    outStats.maxLagMs = tasks_[i].maxLagMs;
#else
    outStats.lastLagMs = 0;
    outStats.maxLagMs = 0;
#endif
    outStats.runCount = tasks_[i].runCount;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
    outStats.deadlineMissCount = tasks_[i].deadlineMissCount;
#else
    outStats.deadlineMissCount = 0;
#endif
    outStats.failureCount = tasks_[i].failureCount;
    outStats.consecutiveFailures = tasks_[i].consecutiveFailures;
    outStats.maxRuntimeUs = tasks_[i].maxRuntimeUs;
    outStats.contractFlags = tasks_[i].contractFlags;
    outStats.failureBudget = tasks_[i].failureBudget;
    outStats.panicMode = tasks_[i].panicMode;
    outStats.safeModePriorityFloor = tasks_[i].safeModePriorityFloor;
#if ZEROKERNEL_ENABLE_CAPABILITIES
    outStats.requiredCapabilities = tasks_[i].requiredCapabilities;
#else
    outStats.requiredCapabilities = kCapNone;
#endif
    outStats.state = tasks_[i].state;
    ++count;
  }

  return count;
}

uint8_t Kernel::snapshotTrace(TraceEntry* buffer, uint8_t capacity) const {
#if !ZEROKERNEL_ENABLE_TRACE
  (void)buffer;
  (void)capacity;
  return 0;
#else
  if (buffer == NULL || capacity == 0) {
    return 0;
  }

  uint8_t count = 0;
  uint8_t index = traceHead_;

  while (count < traceCount_ && count < capacity) {
    buffer[count] = traceBuffer_[index];
    index = static_cast<uint8_t>((index + 1) % kTraceStorage);
    ++count;
  }

  return count;
#endif
}

Kernel::TimeMs Kernel::nextWakeInMs() const {
  if (!started_) {
    return 0;
  }

  if (eventQueueCount_ > 0 || commandQueueCount_ > 0 || workQueueCount_ > 0) {
    return 0;
  }

  const TimeMs nowMs = clockSource_ != NULL ? currentTime_() : lastTickAtMs_;
  TimeMs nextDelayMs = 0;
  bool found = false;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    const TaskSlot& slot = tasks_[i];
    if (!slot.inUse || slot.state != kTaskReady) {
      continue;
    }

    if (safeMode_ && slot.priority < safeModePriorityFloor_) {
      continue;
    }

    const TimeMs elapsedMs = nowMs - slot.lastRunAtMs;
    const TimeMs remainingMs = elapsedMs >= slot.intervalMs ? 0 : (slot.intervalMs - elapsedMs);

    if (!found || remainingMs < nextDelayMs) {
      nextDelayMs = remainingMs;
      found = true;
    }
  }

  return found ? nextDelayMs : 0;
}

uint8_t Kernel::traceCount() const {
#if !ZEROKERNEL_ENABLE_TRACE
  return 0;
#else
  return traceCount_;
#endif
}

void Kernel::clearTrace() {
#if ZEROKERNEL_ENABLE_TRACE
  resetTrace_();
#endif
}

uint8_t Kernel::taskCount() const {
  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (tasks_[i].inUse) {
      ++count;
    }
  }

  return count;
}

uint8_t Kernel::activeTaskCount() const {
  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (tasks_[i].inUse && tasks_[i].state == kTaskReady) {
      ++count;
    }
  }

  return count;
}

int Kernel::findTaskIndex_(const char* name) const {
  if (name == NULL) {
    return -1;
  }

  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (!tasks_[i].inUse) {
      continue;
    }

    if (internal::labelsEqual(labelOrEmpty_(tasks_[i].name), name, kTaskNameLength)) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

int Kernel::findFreeTaskIndex_() const {
  for (uint8_t i = 0; i < kMaxTasks; ++i) {
    if (!tasks_[i].inUse) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

}  // namespace zerokernel
