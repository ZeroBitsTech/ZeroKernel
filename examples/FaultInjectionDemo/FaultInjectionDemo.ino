#include <ZeroKernel.h>
#include <adapters/ArduinoClock.h>

namespace {

const zerokernel::Kernel::TopicKey kHealthKey =
    zerokernel::Kernel::makeTopicKey("fault.health");
const zerokernel::Kernel::TopicKey kFaultKey =
    zerokernel::Kernel::makeTopicKey("fault.injected");

volatile unsigned long gSafeModeEnteredAtMs = 0;
uint8_t gFaultBurstRemaining = 3;
unsigned long gHealthCount = 0;
bool gManualSafeModeTriggered = false;

const char* stateName(uint8_t state) {
  switch (state) {
    case zerokernel::Kernel::kStateBoot:
      return "BOOT";
    case zerokernel::Kernel::kStateNormal:
      return "NORMAL";
    case zerokernel::Kernel::kStateDegraded:
      return "DEGRADED";
    case zerokernel::Kernel::kStateSafeMode:
      return "SAFE_MODE";
    case zerokernel::Kernel::kStateRecovery:
      return "RECOVERY";
    case zerokernel::Kernel::kStatePanic:
      return "PANIC";
    default:
      return "UNKNOWN";
  }
}

const char* signalName(uint8_t signalType) {
  switch (signalType) {
    case zerokernel::Kernel::kSignalTaskFailure:
      return "TASK_FAILURE";
    case zerokernel::Kernel::kSignalHeartbeatTimeout:
      return "HEARTBEAT_TIMEOUT";
    case zerokernel::Kernel::kSignalDeadlineMiss:
      return "DEADLINE_MISS";
    case zerokernel::Kernel::kSignalEventDrop:
      return "EVENT_DROP";
    case zerokernel::Kernel::kSignalExecutionOverrun:
      return "OVERRUN";
    case zerokernel::Kernel::kSignalCommandDrop:
      return "COMMAND_DROP";
    case zerokernel::Kernel::kSignalWorkDrop:
      return "WORK_DROP";
    default:
      return "UNKNOWN";
  }
}

void onStateChange(uint8_t state) {
  Serial.print("STATE ");
  Serial.println(stateName(state));

  if (state == zerokernel::Kernel::kStateSafeMode) {
    gSafeModeEnteredAtMs = millis();
  }
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  Serial.print("SIGNAL ");
  Serial.print(signalName(signal.type));
  Serial.print(" label=");
  Serial.print(signal.label);
  Serial.print(" value=");
  Serial.println(signal.value);
}

void onHealth(const char*, long value) {
  Serial.print("HEALTH beat=");
  Serial.println(value);
}

void onFaultEvent(const char*, long value) {
  Serial.print("FAULT burst_remaining=");
  Serial.println(value);
}

void healthyHeartbeat() {
  ++gHealthCount;
  ZeroKernel.publishDeferredFast(kHealthKey, static_cast<long>(gHealthCount));
}

void unstableWorker() {
  if (gFaultBurstRemaining > 0) {
    ZeroKernel.publishDeferredFast(kFaultKey, static_cast<long>(gFaultBurstRemaining));
    --gFaultBurstRemaining;
    delay(12);
    return;
  }

  ZeroKernel.publishDeferredFast(kFaultKey, 0);
}

void recoverySupervisor() {
  if (!ZeroKernel.isSafeMode()) {
    const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
    if (!gManualSafeModeTriggered && stats.taskFailures >= 3) {
      gManualSafeModeTriggered = true;
      gSafeModeEnteredAtMs = millis();
      Serial.println("RECOVERY entering safe mode");
      ZeroKernel.enterSafeMode(zerokernel::Kernel::kPriorityHigh);
    }
    return;
  }

  if ((millis() - gSafeModeEnteredAtMs) < 2500UL) {
    return;
  }

  Serial.println("RECOVERY resuming normal mode");
  gFaultBurstRemaining = 0;
  ZeroKernel.exitSafeMode();
  ZeroKernel.restartTask("FaultyWorker");
}

void reportRuntime() {
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();

  Serial.print("REPORT state=");
  Serial.print(stateName(ZeroKernel.state()));
  Serial.print(" failures=");
  Serial.print(stats.taskFailures);
  Serial.print(" recoveries=");
  Serial.print(stats.taskRecoveries);
  Serial.print(" overruns=");
  Serial.print(stats.executionOverruns);
  Serial.print(" runs=");
  Serial.println(stats.taskExecutions);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  ZeroKernel.begin(zerokernel::adapters::arduinoMillisClock);
  ZeroKernel.setSignalHandler(onSignal);
  ZeroKernel.onStateChange(onStateChange);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleYield);

  zerokernel::Kernel::WatchdogPolicy policy = {50, 2, true};
  ZeroKernel.setWatchdogPolicy(policy);

  ZeroKernel.subscribeFast(kHealthKey, onHealth, "fault.health");
  ZeroKernel.subscribeFast(kFaultKey, onFaultEvent, "fault.injected");

  zerokernel::Kernel::ExecutionContract unstableContract = {};
  unstableContract.flags = zerokernel::Kernel::kContractAllowDegrade;
  unstableContract.failureBudget = 2;
  unstableContract.safeModePriorityFloor = zerokernel::Kernel::kPriorityHigh;

  zerokernel::Kernel::TaskConfig heartbeatTask = {
      "Heartbeat",
      healthyHeartbeat,
      500,
      5,
      0,
      zerokernel::Kernel::kPriorityHigh,
      true,
      {},
      zerokernel::Kernel::kCapNone};
  zerokernel::Kernel::TaskConfig faultTask = {
      "FaultyWorker",
      unstableWorker,
      700,
      2,
      0,
      zerokernel::Kernel::kPriorityNormal,
      true,
      unstableContract,
      zerokernel::Kernel::kCapNone};
  zerokernel::Kernel::TaskConfig recoveryTask = {
      "Recovery",
      recoverySupervisor,
      500,
      5,
      0,
      zerokernel::Kernel::kPriorityCritical,
      true,
      {},
      zerokernel::Kernel::kCapNone};
  zerokernel::Kernel::TaskConfig reportTask = {
      "Reporter",
      reportRuntime,
      1500,
      10,
      0,
      zerokernel::Kernel::kPriorityHigh,
      true,
      {},
      zerokernel::Kernel::kCapNone};

  ZeroKernel.addTask(heartbeatTask);
  ZeroKernel.addTask(faultTask);
  ZeroKernel.addTask(recoveryTask);
  ZeroKernel.addTask(reportTask);
}

void loop() {
  ZeroKernel.tick();
}
