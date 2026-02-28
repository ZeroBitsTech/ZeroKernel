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
unsigned long gSummaryStartedAtMs = 0;
unsigned long gExpectedHeartbeatAtUs = 0;
uint32_t gHeartbeatRuns = 0;
uint32_t gFaultRuns = 0;
uint32_t gFastMiss = 0;
uint32_t gFastMaxLagUs = 0;
uint64_t gFastLagTotalUs = 0;

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

void resetWindow(unsigned long nowMs) {
  gHeartbeatRuns = 0;
  gFaultRuns = 0;
  gFastMiss = 0;
  gFastMaxLagUs = 0;
  gFastLagTotalUs = 0;
  gSummaryStartedAtMs = nowMs;
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
  const unsigned long nowUs = micros();
  if (gExpectedHeartbeatAtUs == 0UL) {
    gExpectedHeartbeatAtUs = nowUs;
  } else {
    const unsigned long lagUs =
        nowUs > gExpectedHeartbeatAtUs ? (nowUs - gExpectedHeartbeatAtUs) : 0UL;
    gFastLagTotalUs += lagUs;
    if (lagUs > gFastMaxLagUs) {
      gFastMaxLagUs = lagUs;
    }
    if (lagUs > 2000UL) {
      ++gFastMiss;
    }
  }

  gExpectedHeartbeatAtUs += 500000UL;
  ++gHeartbeatRuns;
  ++gHealthCount;
  ZeroKernel.publishDeferredFast(kHealthKey, static_cast<long>(gHealthCount));
}

void unstableWorker() {
  ++gFaultRuns;
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
  const unsigned long nowMs = millis();

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

  if ((nowMs - gSummaryStartedAtMs) < 5000UL) {
    return;
  }

  const uint32_t averageLagUs =
      gHeartbeatRuns == 0 ? 0 : static_cast<uint32_t>(gFastLagTotalUs / gHeartbeatRuns);

  Serial.print("ZEROKERNEL_FAULT window_ms=");
  Serial.print(nowMs - gSummaryStartedAtMs);
  Serial.print(" heartbeat_runs=");
  Serial.print(gHeartbeatRuns);
  Serial.print(" fault_runs=");
  Serial.print(gFaultRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(averageLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(gFastMaxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(gFastMiss);
  Serial.print(" failures=");
  Serial.print(stats.taskFailures);
  Serial.print(" recoveries=");
  Serial.print(stats.taskRecoveries);
  Serial.print(" overruns=");
  Serial.print(stats.executionOverruns);
  Serial.print(" state=");
  Serial.println(stateName(ZeroKernel.state()));

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  ZeroKernel.begin(zerokernel::adapters::arduinoMillisClock);
  resetWindow(millis());
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
