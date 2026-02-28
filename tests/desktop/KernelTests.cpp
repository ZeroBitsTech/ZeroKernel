#include <stdio.h>

#include "ZeroKernel.h"
#include "modules/net/ZeroHttpPump.h"
#include "modules/net/ZeroMqttPump.h"
#include "modules/net/ZeroTransportMetrics.h"
#include "modules/net/ZeroWiFiMaintainer.h"

namespace {

int g_failures = 0;
int g_directEvents = 0;
int g_queuedEvents = 0;
int g_typedEvents = 0;
int g_taskRuns = 0;
int g_signalEvents = 0;
int g_watchdogFeeds = 0;
int g_dumpLines = 0;
int g_commandRuns = 0;
int g_fastEvents = 0;
int g_workRuns = 0;
int g_backpressureCount = 0;
int g_stateChanges = 0;
int g_wifiConnectCalls = 0;
int g_wifiDisconnectCalls = 0;
int g_wifiStateWrites = 0;
int g_wifiStateEvents = 0;
int g_httpConnectCalls = 0;
int g_httpWriteCalls = 0;
int g_httpReadCalls = 0;
int g_httpCloseCalls = 0;
int g_httpCompletionEvents = 0;
int g_mqttConnectCalls = 0;
int g_mqttLoopCalls = 0;
int g_mqttPublishCalls = 0;
int g_mqttStateEvents = 0;
unsigned long g_fakeNowMs = 0;
uint8_t g_lastState = 0;
bool g_wifiLinkUp = false;
bool g_wifiLastState = false;
bool g_httpLastCompletion = false;
bool g_mqttLinkUp = false;
bool g_mqttLastState = false;
bool g_mqttPublishShouldFail = false;
unsigned long g_wifiLastWriteAtMs = 0;
unsigned long g_lastCommandValue = 0;
unsigned long g_lastFastValue = 0;
long g_backpressureFirst = -1;
long g_backpressureLast = -1;
char g_orderLog[8];
int g_orderIndex = 0;

void expectTrue(bool condition, const char* message) {
  if (!condition) {
    ++g_failures;
    printf("FAIL: %s\n", message);
  }
}

void onDirectEvent(const char*, long value) {
  if (value == 7) {
    ++g_directEvents;
  }
}

void onQueuedEvent(const char*, long value) {
  if (value == 9) {
    ++g_queuedEvents;
  }
}

void onTypedEvent(const char*, const zerokernel::Kernel::EventValue& value) {
  if (value.type == zerokernel::Kernel::kEventBool && value.boolValue) {
    ++g_typedEvents;
  }

  if (value.type == zerokernel::Kernel::kEventUnsigned && value.unsignedValue == 99UL) {
    ++g_typedEvents;
  }
}

void onWifiStateEvent(const char*, const zerokernel::Kernel::EventValue& value) {
  if (value.type == zerokernel::Kernel::kEventBool) {
    g_wifiLastState = value.boolValue;
    ++g_wifiStateEvents;
  }
}

void onHttpCompletionEvent(const char*, const zerokernel::Kernel::EventValue& value) {
  if (value.type == zerokernel::Kernel::kEventBool) {
    g_httpLastCompletion = value.boolValue;
    ++g_httpCompletionEvents;
  }
}

void onMqttStateEvent(const char*, const zerokernel::Kernel::EventValue& value) {
  if (value.type == zerokernel::Kernel::kEventBool) {
    g_mqttLastState = value.boolValue;
    ++g_mqttStateEvents;
  }
}

void onCommand(const char*, const zerokernel::Kernel::EventValue& value) {
  ++g_commandRuns;

  if (value.type == zerokernel::Kernel::kEventUnsigned) {
    g_lastCommandValue = value.unsignedValue;
  } else if (value.type == zerokernel::Kernel::kEventLong) {
    g_lastCommandValue = static_cast<unsigned long>(value.longValue);
  }
}

void onFastEvent(const char*, long value) {
  ++g_fastEvents;
  g_lastFastValue = static_cast<unsigned long>(value);
}

void onBackpressureEvent(const char*, long value) {
  if (g_backpressureFirst < 0) {
    g_backpressureFirst = value;
  }

  g_backpressureLast = value;
  ++g_backpressureCount;
}

void onWork(const zerokernel::Kernel::EventValue& value) {
  ++g_workRuns;

  if (value.type == zerokernel::Kernel::kEventUnsigned) {
    g_lastFastValue = value.unsignedValue;
  }
}

unsigned long fakeClock() {
  return g_fakeNowMs;
}

void periodicTask() {
  ++g_taskRuns;
}

void highPriorityTask() {
  if (g_orderIndex < 8) {
    g_orderLog[g_orderIndex++] = 'H';
  }
}

void lowPriorityTask() {
  if (g_orderIndex < 8) {
    g_orderLog[g_orderIndex++] = 'L';
  }
}

void networkCapabilityTask() {
  if (g_orderIndex < 8) {
    g_orderLog[g_orderIndex++] = 'N';
  }
}

void storageCapabilityTask() {
  if (g_orderIndex < 8) {
    g_orderLog[g_orderIndex++] = 'S';
  }
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  if (signal.type == zerokernel::Kernel::kSignalDeadlineMiss ||
      signal.type == zerokernel::Kernel::kSignalEventDrop ||
      signal.type == zerokernel::Kernel::kSignalCommandDrop ||
      signal.type == zerokernel::Kernel::kSignalWorkDrop ||
      signal.type == zerokernel::Kernel::kSignalTaskFailure ||
      signal.type == zerokernel::Kernel::kSignalHeartbeatTimeout ||
      signal.type == zerokernel::Kernel::kSignalExecutionOverrun) {
    ++g_signalEvents;
  }
}

void onHardwareWatchdogFeed() {
  ++g_watchdogFeeds;
}

void onDumpLine(const char*) {
  ++g_dumpLines;
}

void onStateChange(uint8_t state) {
  ++g_stateChanges;
  g_lastState = state;
}

bool wifiLinkProbe() {
  return g_wifiLinkUp;
}

void wifiConnectStep() {
  ++g_wifiConnectCalls;
}

void wifiDisconnectStep() {
  ++g_wifiDisconnectCalls;
}

void wifiStateWriter(bool connected, unsigned long nowMs) {
  g_wifiLastState = connected;
  g_wifiLastWriteAtMs = nowMs;
  ++g_wifiStateWrites;
}

zerokernel::modules::net::ZeroHttpPump::StepResult httpConnectStep(
    const zerokernel::modules::net::ZeroHttpPump::Request&,
    void*) {
  ++g_httpConnectCalls;
  if (g_httpConnectCalls == 1) {
    return zerokernel::modules::net::ZeroHttpPump::kStepPending;
  }
  return zerokernel::modules::net::ZeroHttpPump::kStepComplete;
}

zerokernel::modules::net::ZeroHttpPump::StepResult httpWriteStep(
    const zerokernel::modules::net::ZeroHttpPump::Request&,
    void*) {
  ++g_httpWriteCalls;
  return zerokernel::modules::net::ZeroHttpPump::kStepComplete;
}

zerokernel::modules::net::ZeroHttpPump::StepResult httpReadStep(
    const zerokernel::modules::net::ZeroHttpPump::Request&,
    void*) {
  ++g_httpReadCalls;
  if (g_httpReadCalls == 1) {
    return zerokernel::modules::net::ZeroHttpPump::kStepFailed;
  }
  return zerokernel::modules::net::ZeroHttpPump::kStepComplete;
}

zerokernel::modules::net::ZeroHttpPump::StepResult httpCloseStep(
    const zerokernel::modules::net::ZeroHttpPump::Request&,
    void*) {
  ++g_httpCloseCalls;
  return zerokernel::modules::net::ZeroHttpPump::kStepComplete;
}

bool mqttLinkProbe() {
  return g_mqttLinkUp;
}

bool mqttConnectStep(void*) {
  ++g_mqttConnectCalls;
  g_mqttLinkUp = true;
  return true;
}

void mqttLoopStep(void*) {
  ++g_mqttLoopCalls;
}

bool mqttPublishStep(zerokernel::Kernel::TopicKey,
                     const zerokernel::Kernel::EventValue&,
                     void*) {
  ++g_mqttPublishCalls;
  if (g_mqttPublishShouldFail) {
    g_mqttPublishShouldFail = false;
    return false;
  }
  return true;
}

void longRunningTask() {
  ++g_taskRuns;
  g_fakeNowMs += 5;
}

int testDirectPublish() {
  expectTrue(ZeroKernel.subscribe("direct.topic", onDirectEvent), "subscribe direct");
  expectTrue(ZeroKernel.publish("direct.topic", 7), "publish direct");
  expectTrue(g_directEvents == 1, "direct event delivered");
  expectTrue(ZeroKernel.unsubscribe("direct.topic", onDirectEvent), "unsubscribe direct");
  expectTrue(ZeroKernel.subscriptionCount() == 0, "no active subscriptions after unsubscribe");
  return g_failures;
}

int testDeferredPublish() {
  expectTrue(ZeroKernel.subscribe("queue.topic", onQueuedEvent), "subscribe queue");
  expectTrue(ZeroKernel.publishDeferred("queue.topic", 9), "enqueue deferred event");
  expectTrue(ZeroKernel.publishDeferred("queue.topic", 9), "coalesce duplicate event");
  expectTrue(ZeroKernel.queuedEventCount() == 1, "queued event coalesced");
  ZeroKernel.flushEvents();
  expectTrue(g_queuedEvents == 1, "deferred event delivered");
  expectTrue(ZeroKernel.queuedEventCount() == 0, "queue empty after flush");
  expectTrue(ZeroKernel.unsubscribe("queue.topic"), "unsubscribe queue");
  return g_failures;
}

int testTypedPublish() {
  expectTrue(ZeroKernel.subscribeTyped("typed.topic", onTypedEvent), "subscribe typed");
  expectTrue(ZeroKernel.publishTyped("typed.topic",
                                     zerokernel::Kernel::EventValue::fromBool(true)),
             "publish typed bool");
  expectTrue(ZeroKernel.publishDeferredTyped(
                 "typed.topic",
                 zerokernel::Kernel::EventValue::fromUnsigned(99UL)),
             "publish deferred typed unsigned");
  expectTrue(ZeroKernel.queuedEventCount() == 1, "typed deferred event queued");
  ZeroKernel.flushEvents();
  expectTrue(g_typedEvents == 2, "typed events delivered");
  expectTrue(ZeroKernel.typedSubscriptionCount() == 1, "typed subscription count");
  expectTrue(ZeroKernel.unsubscribeTyped("typed.topic", onTypedEvent), "unsubscribe typed");
  expectTrue(ZeroKernel.typedSubscriptionCount() == 0, "typed subscriptions cleared");
  return g_failures;
}

int testFastDispatch() {
  zerokernel::Kernel isolatedKernel;
  const zerokernel::Kernel::TopicKey topicKey =
      zerokernel::Kernel::makeTopicKey("fast.topic");

  g_fastEvents = 0;
  g_lastFastValue = 0;

  expectTrue(isolatedKernel.subscribe("fast.topic", onFastEvent), "subscribe fast");
  expectTrue(isolatedKernel.publishFast(topicKey, 17), "publish fast by key");
  expectTrue(isolatedKernel.publishDeferredFast(topicKey, 19), "publish deferred fast by key");
  expectTrue(isolatedKernel.queuedEventCount() == 1, "fast deferred event queued");
  isolatedKernel.flushEvents();

  expectTrue(g_fastEvents == 2, "fast events delivered");
  expectTrue(g_lastFastValue == 19UL, "fast deferred event delivered last");

  g_fastEvents = 0;
  expectTrue(isolatedKernel.unsubscribe("fast.topic", onFastEvent), "unsubscribe string fast");
  expectTrue(isolatedKernel.subscribeFast(topicKey, onFastEvent), "subscribe by key only");
  expectTrue(isolatedKernel.publish("fast.topic", 23), "string publish reaches key-only sub");
  expectTrue(g_fastEvents == 1, "key-only subscriber receives string publish");
  expectTrue(isolatedKernel.unsubscribeFast(topicKey, onFastEvent), "unsubscribe by key");
  return g_failures;
}

int testCommandQueue() {
  zerokernel::Kernel isolatedKernel;
  g_commandRuns = 0;
  g_lastCommandValue = 0;

  expectTrue(isolatedKernel.registerCommand("cmd.refresh", onCommand), "register command");
  expectTrue(isolatedKernel.enqueueCommandTyped(
                 "cmd.refresh",
                 zerokernel::Kernel::EventValue::fromUnsigned(41UL)),
             "enqueue command");
  expectTrue(isolatedKernel.enqueueCommandTyped(
                 "cmd.refresh",
                 zerokernel::Kernel::EventValue::fromUnsigned(41UL)),
             "coalesce duplicate command");
  expectTrue(isolatedKernel.enqueueCommand("cmd.refresh", 77), "enqueue distinct command");
  expectTrue(isolatedKernel.queuedCommandCount() == 2, "command queue coalesces duplicates");

  isolatedKernel.flushCommands();

  expectTrue(g_commandRuns == 2, "command handler executed twice");
  expectTrue(g_lastCommandValue == 77UL, "latest command payload delivered");
  expectTrue(isolatedKernel.queuedCommandCount() == 0, "command queue drained");
  expectTrue(isolatedKernel.commandHandlerCount() == 1, "command handler count tracked");
  expectTrue(isolatedKernel.unregisterCommand("cmd.refresh", onCommand), "unregister command");
  expectTrue(isolatedKernel.commandHandlerCount() == 0, "command handler removed");

  const zerokernel::Kernel::KernelStats stats = isolatedKernel.getStats();
  expectTrue(stats.commandsQueued == 2, "command stats track enqueues");
  expectTrue(stats.commandsDelivered == 2, "command stats track deliveries");
  return g_failures;
}

int testWorkQueue() {
  zerokernel::Kernel isolatedKernel;

  g_workRuns = 0;
  g_lastFastValue = 0;

  expectTrue(isolatedKernel.scheduleWorkTyped(
                 onWork, zerokernel::Kernel::EventValue::fromUnsigned(55UL)),
             "schedule work");
  expectTrue(isolatedKernel.scheduleWorkTyped(
                 onWork, zerokernel::Kernel::EventValue::fromUnsigned(55UL)),
             "coalesce duplicate work");
  expectTrue(isolatedKernel.scheduleWork(onWork), "schedule second work");
  expectTrue(isolatedKernel.queuedWorkCount() == 2, "work queue coalesces duplicates");

  isolatedKernel.flushWork();

  expectTrue(g_workRuns == 2, "work handler executed twice");
  expectTrue(g_lastFastValue == 55UL, "work payload preserved");
  expectTrue(isolatedKernel.queuedWorkCount() == 0, "work queue drained");

  const zerokernel::Kernel::KernelStats stats = isolatedKernel.getStats();
  expectTrue(stats.workQueued == 2, "work stats track enqueues");
  expectTrue(stats.workDelivered == 2, "work stats track deliveries");
  return g_failures;
}

int testEventFlags() {
  zerokernel::Kernel isolatedKernel;

  isolatedKernel.setFlags(0x03U);
  expectTrue(isolatedKernel.hasFlags(0x01U), "single flag reported");
  expectTrue(isolatedKernel.hasFlags(0x03U), "combined flags reported");
  expectTrue(!isolatedKernel.hasFlags(0x07U), "missing flag reported");
  expectTrue(isolatedKernel.hasFlags(0x06U, false), "partial flag match reported");

  const zerokernel::Kernel::EventFlags taken = isolatedKernel.takeFlags(0x02U);
  expectTrue(taken == 0x02U, "takeFlags returns captured bits");
  expectTrue(!isolatedKernel.hasFlags(0x02U), "takeFlags clears captured bits");

  isolatedKernel.clearFlags(0x01U);
  expectTrue(!isolatedKernel.hasFlags(0x01U), "clearFlags clears bits");
  return g_failures;
}

int testHeartbeatTimeout() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::WatchdogPolicy policy = {10, 2, true};
  isolatedKernel.setWatchdogPolicy(policy);
  expectTrue(isolatedKernel.addTask("Pulse", periodicTask, 100, 0), "add task");
  expectTrue(isolatedKernel.setTaskHeartbeatTimeout("Pulse", 10), "set per-task heartbeat timeout");

  isolatedKernel.tick(0);
  isolatedKernel.tick(200);

  zerokernel::Kernel::TaskStats stats;
  expectTrue(isolatedKernel.getTaskStats("Pulse", stats), "read task stats");
  expectTrue(stats.failureCount == 1, "heartbeat timeout increments failure count");

  zerokernel::Kernel::KernelStats kernelStats = isolatedKernel.getStats();
  expectTrue(kernelStats.heartbeatTimeouts == 1, "kernel recorded heartbeat timeout");
  return g_failures;
}

int testTaskScheduling() {
  zerokernel::Kernel isolatedKernel;
  expectTrue(isolatedKernel.addTask("Runner", periodicTask, 50, 0), "add periodic task");

  isolatedKernel.tick(0);
  isolatedKernel.tick(50);
  isolatedKernel.tick(100);

  expectTrue(g_taskRuns >= 2, "task executed on schedule");
  return g_failures;
}

int testNextWakeHint() {
  zerokernel::Kernel isolatedKernel;
  expectTrue(isolatedKernel.addTask("Wake", periodicTask, 50, 0), "add wake task");

  isolatedKernel.tick(0);
  expectTrue(isolatedKernel.nextWakeInMs() == 50, "next wake reflects task interval");
  isolatedKernel.tick(20);
  expectTrue(isolatedKernel.nextWakeInMs() == 30, "next wake counts down");
  isolatedKernel.tick(50);
  expectTrue(isolatedKernel.nextWakeInMs() == 50, "next wake resets after execution");
  return g_failures;
}

int testPriorityScheduling() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::TaskConfig low = {
      "Low", lowPriorityTask, 10, 0, 0, zerokernel::Kernel::kPriorityLow, true, {}, zerokernel::Kernel::kCapNone};
  zerokernel::Kernel::TaskConfig high = {
      "High", highPriorityTask, 10, 0, 0, zerokernel::Kernel::kPriorityCritical, true, {}, zerokernel::Kernel::kCapNone};

  g_orderIndex = 0;
  g_orderLog[0] = '\0';

  expectTrue(isolatedKernel.addTask(low), "add low priority task");
  expectTrue(isolatedKernel.addTask(high), "add high priority task");

  isolatedKernel.tick(0);
  isolatedKernel.tick(10);

  expectTrue(g_orderIndex >= 2, "both priority tasks executed");
  expectTrue(g_orderLog[0] == 'H', "high priority task ran first");
  expectTrue(g_orderLog[1] == 'L', "low priority task ran second");

  zerokernel::Kernel::TaskStats highStats;
  expectTrue(isolatedKernel.getTaskStats("High", highStats), "read high priority stats");
  expectTrue(highStats.priority == zerokernel::Kernel::kPriorityCritical,
             "high task priority recorded");
  return g_failures;
}

int testSafeMode() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::TaskConfig low = {
      "LowSafe", lowPriorityTask, 10, 0, 0, zerokernel::Kernel::kPriorityLow, true, {}, zerokernel::Kernel::kCapNone};
  zerokernel::Kernel::TaskConfig high = {
      "HighSafe", highPriorityTask, 10, 0, 0, zerokernel::Kernel::kPriorityCritical, true, {}, zerokernel::Kernel::kCapNone};

  g_orderIndex = 0;
  g_orderLog[0] = '\0';

  expectTrue(isolatedKernel.addTask(low), "add low safe-mode task");
  expectTrue(isolatedKernel.addTask(high), "add high safe-mode task");

  isolatedKernel.tick(0);
  isolatedKernel.enterSafeMode(zerokernel::Kernel::kPriorityHigh);
  expectTrue(isolatedKernel.isSafeMode(), "safe mode enabled");
  isolatedKernel.tick(10);

  expectTrue(g_orderIndex == 1, "safe mode only ran high-priority task");
  expectTrue(g_orderLog[0] == 'H', "high-priority task kept running in safe mode");

  isolatedKernel.exitSafeMode();
  expectTrue(!isolatedKernel.isSafeMode(), "safe mode disabled");
  isolatedKernel.tick(20);
  expectTrue(g_orderIndex >= 3, "both tasks resumed after safe mode");
  return g_failures;
}

int testGovernanceMetadata() {
  zerokernel::Kernel isolatedKernel;

  expectTrue(isolatedKernel.abiVersion() == 1U, "abi version exposed");
  expectTrue(isolatedKernel.runtimeVersion()[0] != '\0', "runtime version exposed");
  expectTrue(isolatedKernel.state() == zerokernel::Kernel::kStateBoot, "boot state default");
  expectTrue(isolatedKernel.getIdleStrategy() == zerokernel::Kernel::kIdlePlatformHint,
             "default idle strategy");
  expectTrue(isolatedKernel.capabilities() == zerokernel::Kernel::kCapAll,
             "default capabilities enabled");
  expectTrue(isolatedKernel.safeModeCapabilities() == zerokernel::Kernel::kCapAll,
             "default safe-mode capabilities enabled");

  isolatedKernel.setIdleStrategy(zerokernel::Kernel::kIdleYield);
  expectTrue(isolatedKernel.getIdleStrategy() == zerokernel::Kernel::kIdleYield,
             "idle strategy updates");
  return g_failures;
}

int testCapabilityRouting() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::TaskConfig networkTask = {
      "NetworkTask",
      networkCapabilityTask,
      10,
      0,
      0,
      zerokernel::Kernel::kPriorityNormal,
      true,
      {},
      zerokernel::Kernel::kCapNetwork};
  zerokernel::Kernel::TaskConfig storageTask = {
      "StorageTask",
      storageCapabilityTask,
      10,
      0,
      0,
      zerokernel::Kernel::kPriorityNormal,
      true,
      {},
      zerokernel::Kernel::kCapStorage};

  g_orderIndex = 0;
  g_orderLog[0] = '\0';

  isolatedKernel.setCapabilities(zerokernel::Kernel::kCapNetwork |
                                 zerokernel::Kernel::kCapStorage);
  isolatedKernel.setSafeModeCapabilities(zerokernel::Kernel::kCapStorage);

  expectTrue(isolatedKernel.addTask(networkTask), "add network capability task");
  expectTrue(isolatedKernel.addTask(storageTask), "add storage capability task");

  isolatedKernel.tick(0);
  isolatedKernel.tick(10);
  expectTrue(g_orderIndex == 2, "both capability tasks run when enabled");

  g_orderIndex = 0;
  isolatedKernel.disableCapabilities(zerokernel::Kernel::kCapStorage);
  isolatedKernel.tick(20);
  expectTrue(g_orderIndex == 1, "disabled capability blocks task");
  expectTrue(g_orderLog[0] == 'N', "network task still runs");

  g_orderIndex = 0;
  isolatedKernel.enableCapabilities(zerokernel::Kernel::kCapStorage);
  isolatedKernel.enterSafeMode(zerokernel::Kernel::kPriorityLow);
  isolatedKernel.tick(30);
  expectTrue(g_orderIndex == 1, "safe mode capability mask narrows task set");
  expectTrue(g_orderLog[0] == 'S', "safe mode allowed storage task");

  zerokernel::Kernel::TaskStats stats;
  expectTrue(isolatedKernel.getTaskStats("StorageTask", stats), "read capability task stats");
  expectTrue(stats.requiredCapabilities == zerokernel::Kernel::kCapStorage,
             "task stats expose required capabilities");

  isolatedKernel.exitSafeMode();
  return g_failures;
}

int testStateAndPanicFlow() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::ExecutionContract contract = {};
  contract.maxRuntimeUs = 1000;
  contract.flags = zerokernel::Kernel::kContractCritical;
  contract.failureBudget = 1;
  contract.panicMode = zerokernel::Kernel::kPanicRebootCallback;
  contract.safeModePriorityFloor = zerokernel::Kernel::kPriorityCritical;
  zerokernel::Kernel::TaskConfig config = {
      "CriticalTask",
      longRunningTask,
      1,
      0,
      0,
      zerokernel::Kernel::kPriorityCritical,
      true,
      contract,
      zerokernel::Kernel::kCapNone};

  g_fakeNowMs = 0;
  g_stateChanges = 0;
  g_lastState = 0;
  g_taskRuns = 0;

  isolatedKernel.onStateChange(onStateChange);
  expectTrue(isolatedKernel.addTask(config), "add critical contract task");
  isolatedKernel.begin(fakeClock);
  expectTrue(isolatedKernel.state() == zerokernel::Kernel::kStateNormal, "begin enters normal");

  g_fakeNowMs = 1;
  isolatedKernel.tick();
  g_fakeNowMs += 1;
  isolatedKernel.tick();

  expectTrue(g_taskRuns >= 1, "critical task ran");
  expectTrue(isolatedKernel.state() == zerokernel::Kernel::kStatePanic, "critical overrun triggers panic");
  expectTrue(!isolatedKernel.isSafeMode(), "panic non-safe-mode policy skips safe mode");
  expectTrue(g_stateChanges >= 2, "state change callback invoked");

  const zerokernel::Kernel::PanicInfo panicInfo = isolatedKernel.getLastPanic();
  expectTrue(panicInfo.reason == zerokernel::Kernel::kPanicTaskOverrun,
             "panic reason tracks overrun");
  expectTrue(panicInfo.mode == zerokernel::Kernel::kPanicRebootCallback,
             "panic mode respects execution contract");
  expectTrue(panicInfo.taskName[0] == 'C', "panic tracks task name");

  zerokernel::Kernel::TaskStats stats;
  expectTrue(isolatedKernel.getTaskStats("CriticalTask", stats), "read critical task stats");
  expectTrue(stats.failureBudget == 1, "task stats expose failure budget");
  return g_failures;
}

int testTimingReport() {
  zerokernel::Kernel isolatedKernel;
  g_fakeNowMs = 0;
  g_taskRuns = 0;

  expectTrue(isolatedKernel.addTask("Timed", longRunningTask, 1, 0), "add timed task");
  isolatedKernel.begin(fakeClock);
  g_fakeNowMs = 1;
  isolatedKernel.tick();
  g_fakeNowMs += 1;
  isolatedKernel.tick();

  const zerokernel::Kernel::TimingReport report = isolatedKernel.getTimingReport();
  expectTrue(report.totalTicks >= 2, "timing report counts ticks");
  expectTrue(report.totalTaskRuns >= 1, "timing report counts task runs");
  expectTrue(report.worstTaskDurationMs >= 5, "timing report tracks task duration");
  expectTrue(report.worstTickDurationMs >= 5, "timing report tracks tick duration");
  return g_failures;
}

int testDeadlineMetrics() {
  zerokernel::Kernel isolatedKernel;
  expectTrue(isolatedKernel.addTask("Laggy", periodicTask, 50, 0), "add laggy task");

  isolatedKernel.tick(0);
  isolatedKernel.tick(80);

  zerokernel::Kernel::TaskStats stats;
  expectTrue(isolatedKernel.getTaskStats("Laggy", stats), "read laggy stats");
  expectTrue(stats.lastLagMs == 30, "lag metric reflects delayed execution");
  expectTrue(stats.deadlineMissCount == 1, "deadline miss count increments");

  zerokernel::Kernel::KernelStats kernelStats = isolatedKernel.getStats();
  expectTrue(kernelStats.deadlineMisses == 1, "kernel deadline miss count increments");
  return g_failures;
}

int testTaskSnapshot() {
  zerokernel::Kernel isolatedKernel;
  expectTrue(isolatedKernel.addTask("Alpha", periodicTask, 25, 0), "add alpha task");
  expectTrue(isolatedKernel.addTask("Beta", periodicTask, 50, 0), "add beta task");

  zerokernel::Kernel::TaskStats snapshots[2];
  const uint8_t captured = isolatedKernel.snapshotTasks(snapshots, 2);

  expectTrue(captured == 2, "snapshot captured both tasks");
  expectTrue(snapshots[0].name[0] == 'A', "first snapshot contains alpha");
  expectTrue(snapshots[1].name[0] == 'B', "second snapshot contains beta");
  return g_failures;
}

int testSignalHook() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::WatchdogPolicy policy = {5, 2, true};
  isolatedKernel.setWatchdogPolicy(policy);
  isolatedKernel.setSignalHandler(onSignal);
  expectTrue(isolatedKernel.addTask("SignalTask", periodicTask, 10, 0), "add signal task");

  isolatedKernel.tick(0);
  isolatedKernel.tick(30);

  for (int i = 0; i < 17; ++i) {
    isolatedKernel.publishDeferred("drop.topic", i);
  }

  expectTrue(g_signalEvents >= 2, "signal hook captured runtime signals");

  zerokernel::Kernel::TraceEntry traceEntries[4];
  const uint8_t traceCaptured = isolatedKernel.snapshotTrace(traceEntries, 4);
  expectTrue(traceCaptured > 0, "trace buffer captured runtime events");
  expectTrue(isolatedKernel.traceCount() > 0, "trace count reports stored entries");
  isolatedKernel.clearTrace();
  expectTrue(isolatedKernel.traceCount() == 0, "trace buffer clears");
  return g_failures;
}

int testQueueBackpressure() {
  zerokernel::Kernel isolatedKernel;

  g_backpressureCount = 0;
  g_backpressureFirst = -1;
  g_backpressureLast = -1;

  expectTrue(isolatedKernel.subscribe("bp.topic", onBackpressureEvent), "subscribe backpressure");

  for (int value = 1; value <= 17; ++value) {
    expectTrue(isolatedKernel.publishDeferred("bp.topic", value), "enqueue with backpressure");
  }

  expectTrue(isolatedKernel.queuedEventCount() == zerokernel::Kernel::kMaxEventQueue,
             "queue remains bounded");
  isolatedKernel.flushEvents();

  expectTrue(g_backpressureCount == zerokernel::Kernel::kMaxEventQueue,
             "bounded queue delivered expected number of events");
  expectTrue(g_backpressureFirst == 2, "oldest event was dropped first");
  expectTrue(g_backpressureLast == 17, "latest event kept");
  return g_failures;
}

int testHardwareWatchdogBridge() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::Kernel::HardwareWatchdogBridge bridge = {
      onHardwareWatchdogFeed, true, true};
  isolatedKernel.setHardwareWatchdogBridge(bridge);
  expectTrue(isolatedKernel.addTask("BridgeTask", periodicTask, 10, 0), "add bridge task");

  g_watchdogFeeds = 0;
  isolatedKernel.tick(0);
  isolatedKernel.tick(10);

  expectTrue(g_watchdogFeeds >= 3, "hardware watchdog feed invoked on tick and task");
  return g_failures;
}

int testDebugDump() {
  zerokernel::Kernel isolatedKernel;
  expectTrue(isolatedKernel.addTask("DumpTask", periodicTask, 10, 0), "add dump task");
  isolatedKernel.tick(0);
  isolatedKernel.tick(10);

  g_dumpLines = 0;
  isolatedKernel.dumpStats(onDumpLine);
  isolatedKernel.dumpTasks(onDumpLine);
  isolatedKernel.dumpTrace(onDumpLine);

  expectTrue(g_dumpLines >= 2, "debug dumps produced output");
  return g_failures;
}

int testWiFiMaintainer() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::modules::net::ZeroWiFiMaintainer maintainer;
  const zerokernel::Kernel::TopicKey stateTopicKey =
      zerokernel::Kernel::makeTopicKey("wifi.state");

  zerokernel::modules::net::ZeroWiFiMaintainer::Config config;
  config.pollIntervalMs = 100;
  config.retryBaseMs = 50;
  config.retryMaxMs = 200;
  config.emitStateChangesOnly = true;
  config.manageCapabilities = true;
  config.capabilityMask = zerokernel::Kernel::kCapNetwork;
  config.stateTopicKey = stateTopicKey;

  g_fakeNowMs = 0;
  g_wifiLinkUp = false;
  g_wifiConnectCalls = 0;
  g_wifiDisconnectCalls = 0;
  g_wifiStateWrites = 0;
  g_wifiStateEvents = 0;
  g_wifiLastState = false;
  g_wifiLastWriteAtMs = 0;

  isolatedKernel.begin(fakeClock);
  expectTrue(isolatedKernel.subscribeTypedFast(stateTopicKey, onWifiStateEvent),
             "subscribe wifi state fast");
  maintainer.begin(isolatedKernel,
                   wifiLinkProbe,
                   wifiConnectStep,
                   wifiDisconnectStep,
                   config,
                   wifiStateWriter);

  g_fakeNowMs = 1;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(g_wifiConnectCalls == 1, "wifi maintainer attempts first connect");
  expectTrue((isolatedKernel.capabilities() & zerokernel::Kernel::kCapNetwork) == 0,
             "wifi maintainer disables network capability while offline");
  expectTrue(g_wifiStateWrites == 0, "wifi maintainer suppresses initial offline notification");

  g_fakeNowMs = 50;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(g_wifiConnectCalls == 1, "wifi maintainer respects poll interval");

  g_fakeNowMs = 120;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(g_wifiConnectCalls == 2, "wifi maintainer retries after backoff");

  g_wifiLinkUp = true;
  g_fakeNowMs = 250;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(maintainer.isConnected(), "wifi maintainer tracks connected state");
  expectTrue(maintainer.reconnectTransitions() == 1,
             "wifi maintainer counts reconnect transition");
  expectTrue(g_wifiStateWrites == 1, "wifi maintainer writes state on reconnect");
  expectTrue(g_wifiStateEvents == 1, "wifi maintainer publishes reconnect state");
  expectTrue(g_wifiLastState, "wifi maintainer publishes connected state");
  expectTrue(g_wifiLastWriteAtMs == 250, "wifi maintainer writes timestamp");
  expectTrue((isolatedKernel.capabilities() & zerokernel::Kernel::kCapNetwork) != 0,
             "wifi maintainer restores network capability");

  g_fakeNowMs = 260;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(g_wifiStateWrites == 1, "wifi maintainer suppresses duplicate connected event");

  g_wifiLinkUp = false;
  g_fakeNowMs = 400;
  isolatedKernel.tick();
  maintainer.tick();
  expectTrue(!maintainer.isConnected(), "wifi maintainer detects disconnect");
  expectTrue(g_wifiDisconnectCalls == 1, "wifi maintainer calls disconnect step");
  expectTrue(g_wifiStateWrites == 2, "wifi maintainer emits offline transition");
  expectTrue(g_wifiStateEvents == 2, "wifi maintainer publishes offline transition");
  expectTrue(!g_wifiLastState, "wifi maintainer publishes offline state");
  expectTrue(maintainer.connectAttempts() == 3, "wifi maintainer retries again after disconnect");

  expectTrue(isolatedKernel.unsubscribeTypedFast(stateTopicKey, onWifiStateEvent),
             "unsubscribe wifi state fast");
  return g_failures;
}

int testTransportMetricsModule() {
  zerokernel::modules::net::ZeroTransportMetrics metrics;

  metrics.recordConnectAttempt();
  metrics.recordConnectResult(false, 5);
  metrics.recordConnectAttempt();
  metrics.recordConnectResult(true, 3);
  metrics.recordSendQueued(2);
  metrics.recordSendAttempt();
  metrics.recordSendResult(true, 7, 11);
  metrics.recordQueueDrop();
  metrics.recordBackoffSchedule();
  metrics.recordLoopCall();

  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot snapshot =
      metrics.snapshot();
  expectTrue(snapshot.connectAttempts == 2, "transport metrics count connect attempts");
  expectTrue(snapshot.connectFailures == 1, "transport metrics count connect failures");
  expectTrue(snapshot.connectSuccesses == 1, "transport metrics count connect successes");
  expectTrue(snapshot.sendAttempts == 1, "transport metrics count send attempts");
  expectTrue(snapshot.sendSuccesses == 1, "transport metrics count send successes");
  expectTrue(snapshot.queueDrops == 1, "transport metrics count queue drops");
  expectTrue(snapshot.backoffSchedules == 1, "transport metrics count backoff schedules");
  expectTrue(snapshot.loopCalls == 1, "transport metrics count loop calls");
#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
  expectTrue(snapshot.worstQueueDwellMs == 11, "transport metrics track queue dwell");
#else
  expectTrue(snapshot.worstQueueDwellMs == 0, "transport metrics disable queue dwell in lean mode");
#endif
  expectTrue(snapshot.maxQueueDepth == 2, "transport metrics track queue depth");
  return g_failures;
}

int testHttpPumpModule() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::modules::net::ZeroHttpPump pump;
  const zerokernel::Kernel::TopicKey completionKey =
      zerokernel::Kernel::makeTopicKey("http.done");

  zerokernel::modules::net::ZeroHttpPump::Config config;
  config.retryBaseMs = 20;
  config.retryMaxMs = 40;
  config.maxRetries = 1;
  config.emitCompletionEvents = true;

  g_fakeNowMs = 0;
  g_httpConnectCalls = 0;
  g_httpWriteCalls = 0;
  g_httpReadCalls = 0;
  g_httpCloseCalls = 0;
  g_httpCompletionEvents = 0;
  g_httpLastCompletion = false;

  isolatedKernel.begin(fakeClock);
  expectTrue(isolatedKernel.subscribeTypedFast(completionKey, onHttpCompletionEvent),
             "subscribe http completion");
  pump.begin(isolatedKernel,
             httpConnectStep,
             httpWriteStep,
             httpReadStep,
             httpCloseStep,
             config);

  zerokernel::modules::net::ZeroHttpPump::Request request;
  request.path = "/api/data";
  request.contentType = "application/json";
  request.body = "{}";
  request.bodyLength = 2;
  request.completionTopicKey = completionKey;

  expectTrue(pump.enqueue(request), "enqueue http request");
  g_fakeNowMs = 1;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(pump.isBusy(), "http pump becomes busy");
  expectTrue(pump.phase() == zerokernel::modules::net::ZeroHttpPump::kPhaseConnecting,
             "http pump starts in connect phase");

  g_fakeNowMs = 2;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(pump.phase() == zerokernel::modules::net::ZeroHttpPump::kPhaseConnecting,
             "http pump retries after collapsed phase failure");
  expectTrue(g_httpWriteCalls == 1, "http pump collapses write phase in same tick");
  expectTrue(g_httpReadCalls == 1, "http pump collapses read phase in same tick");

  g_fakeNowMs = 10;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(g_httpConnectCalls == 2, "http pump waits for retry window");

  g_fakeNowMs = 24;
  isolatedKernel.tick();
  pump.tick();

  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot httpMetrics =
      pump.metrics().snapshot();
  expectTrue(!pump.isBusy(), "http pump returns idle after success");
  expectTrue(httpMetrics.connectAttempts == 2, "http pump records retry connect attempt");
  expectTrue(httpMetrics.sendFailures == 1, "http pump records send failure");
  expectTrue(httpMetrics.sendSuccesses == 1, "http pump records send success");
  expectTrue(httpMetrics.backoffSchedules == 1, "http pump records backoff");
  expectTrue(g_httpCompletionEvents == 1, "http pump publishes completion event");
  expectTrue(g_httpLastCompletion, "http pump publishes successful completion");

  const int httpEnqueueCount = 5;
  for (int index = 0; index < httpEnqueueCount; ++index) {
    expectTrue(pump.enqueue(request), "http pump accepts bounded queue item");
  }
  expectTrue(pump.queuedCount() == zerokernel::modules::net::ZeroHttpPump::kQueueCapacity,
             "http pump queue remains bounded");
  const unsigned long expectedHttpDrops =
      httpEnqueueCount > zerokernel::modules::net::ZeroHttpPump::kQueueCapacity
          ? static_cast<unsigned long>(httpEnqueueCount -
                                       zerokernel::modules::net::ZeroHttpPump::kQueueCapacity)
          : 0UL;
  expectTrue(pump.metrics().snapshot().queueDrops == expectedHttpDrops,
             "http pump drops oldest on overflow");

  expectTrue(isolatedKernel.unsubscribeTypedFast(completionKey, onHttpCompletionEvent),
             "unsubscribe http completion");
  return g_failures;
}

int testMqttPumpModule() {
  zerokernel::Kernel isolatedKernel;
  zerokernel::modules::net::ZeroMqttPump pump;
  const zerokernel::Kernel::TopicKey stateKey =
      zerokernel::Kernel::makeTopicKey("mqtt.link");

  zerokernel::modules::net::ZeroMqttPump::Config config;
  config.pollIntervalMs = 0;
  config.retryBaseMs = 5;
  config.retryMaxMs = 10;
  config.maxRetries = 1;
  config.stateTopicKey = stateKey;

  g_fakeNowMs = 0;
  g_mqttConnectCalls = 0;
  g_mqttLoopCalls = 0;
  g_mqttPublishCalls = 0;
  g_mqttStateEvents = 0;
  g_mqttLinkUp = false;
  g_mqttLastState = false;
  g_mqttPublishShouldFail = true;

  isolatedKernel.begin(fakeClock);
  expectTrue(isolatedKernel.subscribeTypedFast(stateKey, onMqttStateEvent),
             "subscribe mqtt state");
  pump.begin(isolatedKernel,
             mqttLinkProbe,
             mqttConnectStep,
             mqttLoopStep,
             mqttPublishStep,
             config);

  expectTrue(pump.enqueue(zerokernel::Kernel::makeTopicKey("mqtt.out"),
                          zerokernel::Kernel::EventValue::fromUnsigned(7UL)),
             "enqueue mqtt message");

  g_fakeNowMs = 1;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(pump.isConnected(), "mqtt pump connects on first retry");
  expectTrue(g_mqttStateEvents == 1, "mqtt pump publishes connected state");
  expectTrue(g_mqttLastState, "mqtt pump reports connected");

  g_fakeNowMs = 2;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(g_mqttPublishCalls == 1, "mqtt pump attempts first publish");
  expectTrue(pump.queuedCount() == 1, "mqtt pump keeps message queued after failure");

  g_fakeNowMs = 4;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(g_mqttPublishCalls == 1, "mqtt pump honors publish backoff");

  g_fakeNowMs = 7;
  isolatedKernel.tick();
  pump.tick();
  expectTrue(g_mqttPublishCalls == 2, "mqtt pump retries publish after backoff");
  expectTrue(pump.queuedCount() == 0, "mqtt pump drains queue after retry success");
  expectTrue(g_mqttLoopCalls >= 2, "mqtt pump calls loop while connected");

  const int mqttEnqueueCount = 7;
  for (int index = 0; index < mqttEnqueueCount; ++index) {
    expectTrue(
        pump.enqueue(zerokernel::Kernel::makeTopicKey("mqtt.out"),
                     zerokernel::Kernel::EventValue::fromLong(index)),
        "enqueue mqtt queue item");
  }

  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot mqttMetrics =
      pump.metrics().snapshot();
  expectTrue(pump.queuedCount() == zerokernel::modules::net::ZeroMqttPump::kQueueCapacity,
             "mqtt pump queue remains bounded");
  expectTrue(mqttMetrics.sendFailures == 1, "mqtt pump records send failure");
  expectTrue(mqttMetrics.sendSuccesses == 1, "mqtt pump records send success");
  const unsigned long expectedMqttDrops =
      mqttEnqueueCount > zerokernel::modules::net::ZeroMqttPump::kQueueCapacity
          ? static_cast<unsigned long>(mqttEnqueueCount -
                                       zerokernel::modules::net::ZeroMqttPump::kQueueCapacity)
          : 0UL;
  expectTrue(mqttMetrics.queueDrops == expectedMqttDrops, "mqtt pump records queue drop");
  expectTrue(mqttMetrics.backoffSchedules == 1, "mqtt pump records publish backoff");

  expectTrue(isolatedKernel.unsubscribeTypedFast(stateKey, onMqttStateEvent),
             "unsubscribe mqtt state");
  return g_failures;
}

}  // namespace

int main() {
  g_failures = 0;
  g_directEvents = 0;
  g_queuedEvents = 0;
  g_typedEvents = 0;
  g_taskRuns = 0;
  g_signalEvents = 0;
  g_watchdogFeeds = 0;
  g_dumpLines = 0;
  g_commandRuns = 0;
  g_fastEvents = 0;
  g_workRuns = 0;
  g_backpressureCount = 0;
  g_stateChanges = 0;
  g_wifiConnectCalls = 0;
  g_wifiDisconnectCalls = 0;
  g_wifiStateWrites = 0;
  g_wifiStateEvents = 0;
  g_httpConnectCalls = 0;
  g_httpWriteCalls = 0;
  g_httpReadCalls = 0;
  g_httpCloseCalls = 0;
  g_httpCompletionEvents = 0;
  g_mqttConnectCalls = 0;
  g_mqttLoopCalls = 0;
  g_mqttPublishCalls = 0;
  g_mqttStateEvents = 0;
  g_fakeNowMs = 0;
  g_lastState = 0;
  g_wifiLinkUp = false;
  g_wifiLastState = false;
  g_httpLastCompletion = false;
  g_mqttLinkUp = false;
  g_mqttLastState = false;
  g_mqttPublishShouldFail = false;
  g_wifiLastWriteAtMs = 0;
  g_lastCommandValue = 0;
  g_lastFastValue = 0;
  g_backpressureFirst = -1;
  g_backpressureLast = -1;
  g_orderIndex = 0;

  testDirectPublish();
  testDeferredPublish();
  testTypedPublish();
  testFastDispatch();
  testCommandQueue();
  testWorkQueue();
  testEventFlags();
  testHeartbeatTimeout();
  testTaskScheduling();
  testNextWakeHint();
  testPriorityScheduling();
  testSafeMode();
  testGovernanceMetadata();
  testCapabilityRouting();
  testStateAndPanicFlow();
  testDeadlineMetrics();
  testTimingReport();
  testTaskSnapshot();
  testSignalHook();
  testQueueBackpressure();
  testHardwareWatchdogBridge();
  testDebugDump();
  testWiFiMaintainer();
  testTransportMetricsModule();
  testHttpPumpModule();
  testMqttPumpModule();

  if (g_failures == 0) {
    printf("All ZeroKernel desktop tests passed.\n");
    return 0;
  }

  printf("%d ZeroKernel desktop tests failed.\n", g_failures);
  return 1;
}
