#ifndef ZEROKERNEL_H
#define ZEROKERNEL_H

#include <stddef.h>
#include <stdint.h>

#include "ZeroKernelConfig.h"

namespace zerokernel {

class Kernel {
 public:
  typedef unsigned long TimeMs;
  typedef uint32_t TopicKey;
  typedef uint32_t EventFlags;
  typedef uint32_t CapabilityMask;
  typedef void (*TaskCallback)();
  typedef void (*EventHandler)(const char* topic, long value);
  struct EventValue;
  typedef void (*TypedEventHandler)(const char* topic, const EventValue& value);
  typedef void (*CommandHandler)(const char* command, const EventValue& value);
  typedef void (*WorkHandler)(const EventValue& value);
  struct KernelSignal;
  struct PanicInfo;
  struct TimingReport;
  typedef void (*SignalHandler)(const KernelSignal& signal);
  typedef void (*StateChangeHandler)(uint8_t state);
  typedef void (*PanicHandler)(const PanicInfo& panicInfo);
  typedef void (*TextWriter)(const char* line);
  typedef TimeMs (*ClockSource)();

  enum TaskState : uint8_t {
    kTaskReady = 0,
    kTaskSuspended = 1,
    kTaskFaulted = 2
  };

  enum TaskPriority : uint8_t {
    kPriorityLow = 0,
    kPriorityNormal = 1,
    kPriorityHigh = 2,
    kPriorityCritical = 3
  };

  enum EventValueType : uint8_t {
    kEventLong = 0,
    kEventUnsigned = 1,
    kEventBool = 2,
    kEventPointer = 3
  };

  enum SignalType : uint8_t {
    kSignalTaskFailure = 0,
    kSignalHeartbeatTimeout = 1,
    kSignalDeadlineMiss = 2,
    kSignalEventDrop = 3,
    kSignalExecutionOverrun = 4,
    kSignalCommandDrop = 5,
    kSignalWorkDrop = 6
  };

  enum KernelState : uint8_t {
    kStateBoot = 0,
    kStateNormal = 1,
    kStateDegraded = 2,
    kStateSafeMode = 3,
    kStateRecovery = 4,
    kStatePanic = 5
  };

  enum IdleStrategy : uint8_t {
    kIdleBusy = 0,
    kIdleYield = 1,
    kIdleSleep = 2,
    kIdlePlatformHint = 3
  };

  enum PanicReason : uint8_t {
    kPanicNone = 0,
    kPanicTaskOverrun = 1,
    kPanicHeartbeatTimeout = 2,
    kPanicTaskFailure = 3,
    kPanicManual = 4
  };

  enum PanicMode : uint8_t {
    kPanicUseDefault = 0,
    kPanicFreeze = 1,
    kPanicEnterSafeMode = 2,
    kPanicRebootCallback = 3
  };

  enum Capability : uint32_t {
    kCapNone = 0U,
    kCapIO = 1U << 0,
    kCapNetwork = 1U << 1,
    kCapStorage = 1U << 2,
    kCapTelemetry = 1U << 3,
    kCapDiagnostics = 1U << 4,
    kCapRadio = 1U << 5,
    kCapControl = 1U << 6,
    kCapCustom0 = 1U << 7,
    kCapAll = 0xFFFFFFFFU
  };

  enum ContractFlag : uint8_t {
    kContractCritical = 1 << 0,
    kContractDropIfLate = 1 << 1,
    kContractAllowDegrade = 1 << 2
  };

  struct EventValue {
    uint8_t type;
    union {
      long longValue;
      unsigned long unsignedValue;
      bool boolValue;
      const void* pointerValue;
    };

    static EventValue fromLong(long value) {
      EventValue payload = {};
      payload.type = kEventLong;
      payload.longValue = value;
      return payload;
    }

    static EventValue fromUnsigned(unsigned long value) {
      EventValue payload = {};
      payload.type = kEventUnsigned;
      payload.unsignedValue = value;
      return payload;
    }

    static EventValue fromBool(bool value) {
      EventValue payload = {};
      payload.type = kEventBool;
      payload.boolValue = value;
      return payload;
    }

    static EventValue fromPointer(const void* value) {
      EventValue payload = {};
      payload.type = kEventPointer;
      payload.pointerValue = value;
      return payload;
    }
  };

  struct KernelSignal {
    uint8_t type;
    char label[24];
    unsigned long value;
  };

  struct ExecutionContract {
    uint16_t maxRuntimeUs;
    uint8_t flags;
    uint8_t failureBudget;
    uint8_t panicMode;
    uint8_t safeModePriorityFloor;
  };

  struct PanicInfo {
    uint8_t reason;
    uint8_t mode;
    char taskName[24];
    unsigned long observedValue;
    unsigned long budgetValue;
  };

  struct TraceEntry {
    TimeMs timestampMs;
    uint8_t type;
    char label[24];
    unsigned long value;
  };

  struct TaskConfig {
    const char* name;
    TaskCallback callback;
    TimeMs intervalMs;
    TimeMs maxRuntimeMs;
    TimeMs heartbeatTimeoutMs;
    uint8_t priority;
    bool startEnabled;
    ExecutionContract contract;
    CapabilityMask requiredCapabilities;
  };

  struct TaskStats {
    char name[24];
    TimeMs intervalMs;
    TimeMs maxRuntimeMs;
    TimeMs heartbeatTimeoutMs;
    uint8_t priority;
    TimeMs lastRunAtMs;
    TimeMs lastDurationMs;
    TimeMs lastHeartbeatAtMs;
    TimeMs lastLagMs;
    TimeMs maxLagMs;
    uint32_t runCount;
    uint32_t deadlineMissCount;
    uint16_t failureCount;
    uint8_t consecutiveFailures;
    uint16_t maxRuntimeUs;
    uint8_t contractFlags;
    uint8_t failureBudget;
    uint8_t panicMode;
    uint8_t safeModePriorityFloor;
    CapabilityMask requiredCapabilities;
    TaskState state;
  };

  struct TimingReport {
    TimeMs worstTickDurationMs;
    TimeMs averageTickDurationMs;
    TimeMs worstTaskDurationMs;
    TimeMs averageTaskDurationMs;
    TimeMs worstLagMs;
    TimeMs averageLagMs;
    uint32_t worstTickCycles;
    uint32_t averageTickCycles;
    uint32_t worstTaskCycles;
    uint32_t averageTaskCycles;
    uint32_t totalTicks;
    uint32_t totalTaskRuns;
    uint32_t deadlineMisses;
  };

  struct KernelStats {
    TimeMs uptimeMs;
    uint32_t schedulerLoops;
    uint32_t schedulerIdleLoops;
    uint32_t taskExecutions;
    uint32_t taskFailures;
    uint32_t taskRecoveries;
    uint32_t deadlineMisses;
    uint32_t eventsPublished;
    uint32_t eventsDelivered;
    uint32_t watchdogTrips;
    uint32_t heartbeatTimeouts;
    uint32_t executionOverruns;
    uint32_t queuedEventsDropped;
    uint32_t commandsQueued;
    uint32_t commandsDelivered;
    uint32_t queuedCommandsDropped;
    uint32_t workQueued;
    uint32_t workDelivered;
    uint32_t queuedWorkDropped;
    uint8_t registeredTasks;
    uint8_t activeTasks;
  };

  struct Identity {
    const char* name;
    const char* vendor;
    const char* tagline;
    const char* site;
    const char* version;
  };

  struct WatchdogPolicy {
    TimeMs heartbeatTimeoutMs;
    uint8_t maxConsecutiveFailures;
    bool autoRecovery;
  };

  struct HardwareWatchdogBridge {
    void (*feed)();
    bool feedOnTick;
    bool feedAfterTask;
  };

  static const uint8_t kMaxTasks = ZEROKERNEL_MAX_TASKS;
  static const uint8_t kMaxSubscribers = ZEROKERNEL_MAX_SUBSCRIBERS;
  static const uint8_t kMaxTypedSubscribers = ZEROKERNEL_MAX_TYPED_SUBSCRIBERS;
  static const uint8_t kMaxEventQueue = ZEROKERNEL_MAX_EVENT_QUEUE;
  static const uint8_t kMaxCommandHandlers = ZEROKERNEL_MAX_COMMAND_HANDLERS;
  static const uint8_t kMaxCommandQueue = ZEROKERNEL_MAX_COMMAND_QUEUE;
  static const uint8_t kMaxWorkQueue = ZEROKERNEL_MAX_WORK_QUEUE;
  static const uint8_t kMaxTraceEntries = ZEROKERNEL_MAX_TRACE_ENTRIES;
  static const uint16_t kAbiVersion = 1;

  Kernel();

  void begin(ClockSource clockSource = NULL);

  bool addTask(const char* name,
               TaskCallback callback,
               TimeMs intervalMs,
               TimeMs maxRuntimeMs = 0,
               bool startEnabled = true);
  bool addTask(const TaskConfig& config);

  void tick();
  void tick(TimeMs nowMs);

  bool suspendTask(const char* name);
  bool resumeTask(const char* name);
  bool restartTask(const char* name);
  bool heartbeatTask(const char* name);
  bool setTaskHeartbeatTimeout(const char* name, TimeMs heartbeatTimeoutMs);
  bool setTaskPriority(const char* name, uint8_t priority);

  void setWatchdogPolicy(const WatchdogPolicy& policy);
  WatchdogPolicy getWatchdogPolicy() const;
  void setSignalHandler(SignalHandler handler);
  void setHardwareWatchdogBridge(const HardwareWatchdogBridge& bridge);
  HardwareWatchdogBridge getHardwareWatchdogBridge() const;
  void setIdleStrategy(uint8_t idleStrategy);
  uint8_t getIdleStrategy() const;
  void setCapabilities(CapabilityMask capabilities);
  CapabilityMask capabilities() const;
  void enableCapabilities(CapabilityMask capabilities);
  void disableCapabilities(CapabilityMask capabilities);
  void setSafeModeCapabilities(CapabilityMask capabilities);
  CapabilityMask safeModeCapabilities() const;
  void onStateChange(StateChangeHandler handler);
  uint8_t state() const;
  void setPanicHandler(PanicHandler handler);
  PanicInfo getLastPanic() const;
  void triggerPanic(uint8_t reason,
                    const char* taskName = NULL,
                    unsigned long observedValue = 0,
                    unsigned long budgetValue = 0,
                    uint8_t mode = kPanicEnterSafeMode);
  void enterSafeMode(uint8_t minimumPriority = kPriorityHigh);
  void exitSafeMode();
  bool isSafeMode() const;
  static TopicKey makeTopicKey(const char* label);

  bool publish(const char* topic, long value);
  bool publishTyped(const char* topic, const EventValue& value);
  bool publishFast(TopicKey topicKey, long value);
  bool publishTypedFast(TopicKey topicKey, const EventValue& value);
  bool publishDeferred(const char* topic, long value);
  bool publishDeferredTyped(const char* topic, const EventValue& value);
  bool publishDeferredFast(TopicKey topicKey, long value);
  bool publishDeferredTypedFast(TopicKey topicKey, const EventValue& value);
  bool enqueueCommand(const char* command, long value);
  bool enqueueCommandTyped(const char* command, const EventValue& value);
  bool enqueueCommandFast(TopicKey commandKey, long value);
  bool enqueueCommandTypedFast(TopicKey commandKey, const EventValue& value);
  bool scheduleWork(WorkHandler handler);
  bool scheduleWorkTyped(WorkHandler handler, const EventValue& value);
  bool subscribe(const char* topic, EventHandler handler);
  bool subscribeFast(TopicKey topicKey, EventHandler handler, const char* topicLabel = NULL);
  bool subscribeTyped(const char* topic, TypedEventHandler handler);
  bool subscribeTypedFast(TopicKey topicKey,
                          TypedEventHandler handler,
                          const char* topicLabel = NULL);
  bool registerCommand(const char* command, CommandHandler handler);
  bool registerCommandFast(TopicKey commandKey,
                           CommandHandler handler,
                           const char* commandLabel = NULL);
  bool unsubscribe(const char* topic, EventHandler handler = NULL);
  bool unsubscribeFast(TopicKey topicKey, EventHandler handler = NULL);
  bool unsubscribeTyped(const char* topic, TypedEventHandler handler = NULL);
  bool unsubscribeTypedFast(TopicKey topicKey, TypedEventHandler handler = NULL);
  bool unregisterCommand(const char* command, CommandHandler handler = NULL);
  bool unregisterCommandFast(TopicKey commandKey, CommandHandler handler = NULL);
  void flushEvents();
  void flushCommands();
  void flushWork();
  void setFlags(EventFlags mask);
  void clearFlags(EventFlags mask);
  bool hasFlags(EventFlags mask, bool requireAll = true) const;
  EventFlags takeFlags(EventFlags mask);

  bool getTaskStats(const char* name, TaskStats& outStats) const;
  uint8_t snapshotTasks(TaskStats* buffer, uint8_t capacity) const;
  uint8_t snapshotTrace(TraceEntry* buffer, uint8_t capacity) const;
  TimeMs nextWakeInMs() const;
  uint8_t traceCount() const;
  void clearTrace();
  void dumpStats(TextWriter writer) const;
  void dumpTasks(TextWriter writer) const;
  void dumpTrace(TextWriter writer) const;
  KernelStats getStats() const;
  TimingReport getTimingReport() const;
  const Identity& identity() const;
  uint16_t abiVersion() const;
  const char* runtimeVersion() const;

  uint8_t taskCount() const;
  uint8_t activeTaskCount() const;
  uint8_t subscriptionCount() const;
  uint8_t typedSubscriptionCount() const;
  uint8_t commandHandlerCount() const;
  uint8_t queuedEventCount() const;
  uint8_t queuedCommandCount() const;
  uint8_t queuedWorkCount() const;

 private:
  static const size_t kTaskNameLength = 24;
  static const size_t kTopicNameLength = 24;
  static const uint8_t kTaskStorage = kMaxTasks > 0 ? kMaxTasks : 1;
  static const uint8_t kSubscriberStorage = kMaxSubscribers > 0 ? kMaxSubscribers : 1;
  static const uint8_t kTypedSubscriberStorage =
      kMaxTypedSubscribers > 0 ? kMaxTypedSubscribers : 1;
  static const uint8_t kEventQueueStorage = kMaxEventQueue > 0 ? kMaxEventQueue : 1;
  static const uint8_t kCommandHandlerStorage =
      kMaxCommandHandlers > 0 ? kMaxCommandHandlers : 1;
  static const uint8_t kCommandQueueStorage =
      kMaxCommandQueue > 0 ? kMaxCommandQueue : 1;
  static const uint8_t kWorkQueueStorage = kMaxWorkQueue > 0 ? kMaxWorkQueue : 1;
  static const uint8_t kTraceStorage = kMaxTraceEntries > 0 ? kMaxTraceEntries : 1;

  struct TaskSlot {
    TaskCallback callback;
    const char* name;
    TimeMs intervalMs;
    TimeMs maxRuntimeMs;
    TimeMs heartbeatTimeoutMs;
    TimeMs lastRunAtMs;
    TimeMs lastDurationMs;
    TimeMs lastHeartbeatAtMs;
#if ZEROKERNEL_ENABLE_EXTENDED_TASK_METRICS
    TimeMs lastLagMs;
    TimeMs maxLagMs;
    uint32_t deadlineMissCount;
#endif
    uint32_t runCount;
    uint16_t maxRuntimeUs;
    CapabilityMask requiredCapabilities;
    uint16_t failureCount;
    uint8_t priority;
    uint8_t contractFlags;
    uint8_t failureBudget;
    uint8_t panicMode;
    uint8_t safeModePriorityFloor;
    uint8_t consecutiveFailures;
    TaskState state;
    bool inUse;
    bool startEnabled;
  };

  struct SubscriptionSlot {
    bool inUse;
    TopicKey topicKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    const char* topic;
#endif
    EventHandler handler;
  };

  #if ZEROKERNEL_ENABLE_TYPED_EVENTS
  struct TypedSubscriptionSlot {
    bool inUse;
    TopicKey topicKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    const char* topic;
#endif
    TypedEventHandler handler;
  };
  #else
  struct TypedSubscriptionSlot {
    uint8_t unused;
  };
  #endif

  struct EventMessage {
    bool inUse;
    TopicKey topicKey;
    EventValue value;
  };

  #if ZEROKERNEL_ENABLE_COMMAND_QUEUE
  struct CommandHandlerSlot {
    bool inUse;
    TopicKey topicKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    const char* topic;
#endif
    CommandHandler handler;
  };

  struct CommandMessage {
    bool inUse;
    TopicKey topicKey;
    EventValue value;
  };
  #else
  struct CommandHandlerSlot {
    uint8_t unused;
  };

  struct CommandMessage {
    uint8_t unused;
  };
  #endif

  #if ZEROKERNEL_ENABLE_WORK_QUEUE
  struct WorkMessage {
    bool inUse;
    WorkHandler handler;
    EventValue value;
  };
  #else
  struct WorkMessage {
    uint8_t unused;
  };
  #endif

  #if ZEROKERNEL_ENABLE_TRACE
  typedef TraceEntry TraceStorageEntry;
  #else
  struct TraceStorageEntry {
    uint8_t unused;
  };
  #endif

  TaskSlot tasks_[kTaskStorage];
  SubscriptionSlot subscribers_[kSubscriberStorage];
  TypedSubscriptionSlot typedSubscribers_[kTypedSubscriberStorage];
  EventMessage eventQueue_[kEventQueueStorage];
  CommandHandlerSlot commandHandlers_[kCommandHandlerStorage];
  CommandMessage commandQueue_[kCommandQueueStorage];
  WorkMessage workQueue_[kWorkQueueStorage];
  TraceStorageEntry traceBuffer_[kTraceStorage];
  ClockSource clockSource_;
  TimeMs bootAtMs_;
  TimeMs lastTickAtMs_;
  bool started_;
  KernelStats kernelStats_;
  WatchdogPolicy watchdogPolicy_;
  HardwareWatchdogBridge hardwareWatchdogBridge_;
  SignalHandler signalHandler_;
  StateChangeHandler stateChangeHandler_;
  PanicHandler panicHandler_;
  uint8_t eventQueueHead_;
  uint8_t eventQueueTail_;
  uint8_t eventQueueCount_;
  uint8_t commandQueueHead_;
  uint8_t commandQueueTail_;
  uint8_t commandQueueCount_;
  uint8_t workQueueHead_;
  uint8_t workQueueTail_;
  uint8_t workQueueCount_;
  uint8_t traceHead_;
  uint8_t traceTail_;
  uint8_t traceCount_;
  volatile EventFlags eventFlags_;
  CapabilityMask capabilityMask_;
  CapabilityMask safeModeCapabilityMask_;
  uint8_t kernelState_;
  bool safeMode_;
  uint8_t safeModePriorityFloor_;
  uint8_t idleStrategy_;
  PanicInfo lastPanic_;
  uint32_t totalTickDurationMs_;
  uint32_t totalTaskDurationMs_;
  uint32_t totalLagMs_;
  uint64_t totalTickCycles_;
  uint64_t totalTaskCycles_;
  TimeMs maxTickDurationMs_;
  TimeMs maxTaskDurationMs_;
  TimeMs maxObservedLagMs_;
  uint32_t maxTickCycles_;
  uint32_t maxTaskCycles_;

  void resetKernelStats_();
  void resetRuntimeState_(TaskSlot& slot, TimeMs nowMs);
  void initializeTaskSlot_(TaskSlot& slot, const TaskConfig& config, TimeMs nowMs);
  void recordExecutionSuccess_(TaskSlot& slot, TimeMs beforeRun, TimeMs afterRun);
  void recordFailure_(TaskSlot& slot, bool heartbeatTimeout);
  void recoverIfAllowed_(TaskSlot& slot, TimeMs nowMs);
  bool detectHeartbeatTimeout_(TaskSlot& slot, TimeMs nowMs);
  bool isTaskDue_(const TaskSlot& slot, TimeMs nowMs) const;
  bool dispatchEvent_(const char* topic, const EventValue& value);
  bool dispatchEventByKey_(TopicKey topicKey, const EventValue& value);
  bool dispatchCommand_(const char* command, const EventValue& value);
  bool dispatchCommandByKey_(TopicKey commandKey, const EventValue& value);
  bool enqueueEvent_(const char* topic, const EventValue& value);
  bool enqueueEventByKey_(TopicKey topicKey, const EventValue& value);
  bool enqueueCommand_(const char* command, const EventValue& value);
  bool enqueueCommandByKey_(TopicKey commandKey, const EventValue& value);
  void resetEventQueue_();
  void drainEventQueue_(uint8_t limit = 0);
  void resetCommandQueue_();
  void drainCommandQueue_(uint8_t limit = 0);
  void resetWorkQueue_();
  bool enqueueWork_(WorkHandler handler, const EventValue& value);
  void drainWorkQueue_(uint8_t limit = 0);
  void resetTrace_();
  void pushTrace_(uint8_t type, const char* label, unsigned long value);
  void serviceHardwareWatchdog_(bool afterTask);
  void inspectHeartbeats_(TimeMs nowMs);
  uint8_t resolveDrainLimit_(uint8_t queuedCount, uint8_t baseLimit, uint8_t capacity) const;
  void setKernelState_(uint8_t nextState);
  bool dropOldestEvent_();
  bool dropOldestCommand_();
  bool dropOldestWork_();
  int selectNextRunnableTask_(TimeMs nowMs) const;
  void emitSignal_(uint8_t type, const char* label, unsigned long value);
  bool hasTaskCapabilities_(const TaskSlot& slot) const;

  int findTaskIndex_(const char* name) const;
  int findFreeTaskIndex_() const;
  int findFreeSubscriberIndex_() const;
  int findFreeTypedSubscriberIndex_() const;
  int findFreeCommandHandlerIndex_() const;

  TimeMs currentTime_() const;
  void copyLabel_(char* destination, size_t length, const char* source) const;
  const char* labelOrEmpty_(const char* source) const;
  TopicKey topicKey_(const char* source) const;
  bool eventValuesEqual_(const EventValue& left, const EventValue& right) const;
};

}  // namespace zerokernel

extern zerokernel::Kernel ZeroKernel;

#endif
