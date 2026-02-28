#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

namespace {

const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kFastMissThresholdUs = 1500UL;

unsigned long gNextExpectedUs = 0;
unsigned long gLagAccumUs = 0;
unsigned long gMaxLagUs = 0;
unsigned long gFastMisses = 0;
unsigned long gControlRuns = 0;
unsigned long gCommandUpdates = 0;
unsigned long gDiagRuns = 0;
unsigned long gSafeModeEntries = 0;
unsigned long gRecoveries = 0;
unsigned long gRecoveryDeadlineMs = 0;
long gTarget = 100;
long gMeasured = 90;
bool gRecoveryPending = false;
const zerokernel::Kernel::TopicKey kCommandKey =
    zerokernel::Kernel::makeTopicKey("industrial.target");
unsigned long gWindowStartedAtMs = 0;

unsigned long boardMillis() {
  return millis();
}

void busyWaitUs(unsigned long durationUs) {
  const unsigned long startedAtUs = micros();
  while ((micros() - startedAtUs) < durationUs) {
  }
}

void resetWindow(unsigned long nowMs) {
  gWindowStartedAtMs = nowMs;
  gLagAccumUs = 0;
  gMaxLagUs = 0;
  gFastMisses = 0;
  gControlRuns = 0;
  gCommandUpdates = 0;
  gDiagRuns = 0;
  gSafeModeEntries = 0;
  gRecoveries = 0;
}

void controlTask() {
  const unsigned long nowUs = micros();
  if (gNextExpectedUs == 0UL) {
    gNextExpectedUs = nowUs;
  }

  const unsigned long lagUs = nowUs > gNextExpectedUs ? (nowUs - gNextExpectedUs) : 0UL;
  gLagAccumUs += lagUs;
  if (lagUs > gMaxLagUs) {
    gMaxLagUs = lagUs;
  }
  if (lagUs > kFastMissThresholdUs) {
    ++gFastMisses;
  }

  ++gControlRuns;
  if (gMeasured < gTarget) {
    ++gMeasured;
  } else if (gMeasured > gTarget) {
    --gMeasured;
  }

  gNextExpectedUs += 50000UL;
  if (nowUs > (gNextExpectedUs + 50000UL)) {
    gNextExpectedUs = nowUs + 50000UL;
  }
}

void onTargetCommand(const char*, const zerokernel::Kernel::EventValue& value) {
  ++gCommandUpdates;
  if (value.type == zerokernel::Kernel::kEventLong) {
    gTarget = value.longValue;
  }
}

void commandTask() {
  static bool toggle = false;
  toggle = !toggle;
  ZeroKernel.enqueueCommandFast(kCommandKey, toggle ? 125L : 100L);
}

void diagnosticsTask() {
  ++gDiagRuns;
  if ((gDiagRuns % 6UL) == 0) {
    busyWaitUs(4000UL);
    ZeroKernel.enterSafeMode(zerokernel::Kernel::kPriorityHigh);
    gRecoveryPending = true;
    gRecoveryDeadlineMs = millis() + 150UL;
    ++gSafeModeEntries;
  } else {
    busyWaitUs(2500UL);
  }
}

void recoveryTask() {
  if (!gRecoveryPending) {
    return;
  }

  if (millis() < gRecoveryDeadlineMs) {
    return;
  }

  ZeroKernel.exitSafeMode();
  gRecoveryPending = false;
  ++gRecoveries;
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  const unsigned long avgLagUs = gControlRuns == 0 ? 0 : (gLagAccumUs / gControlRuns);
  Serial.print("ZEROKERNEL_INDUSTRIAL window_ms=");
  Serial.print(nowMs - gWindowStartedAtMs);
  Serial.print(" control_runs=");
  Serial.print(gControlRuns);
  Serial.print(" command_updates=");
  Serial.print(gCommandUpdates);
  Serial.print(" diag_runs=");
  Serial.print(gDiagRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(gMaxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(gFastMisses);
  Serial.print(" safe_mode_entries=");
  Serial.print(gSafeModeEntries);
  Serial.print(" recoveries=");
  Serial.println(gRecoveries);

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleSleep);
  ZeroKernel.registerCommandFast(kCommandKey, onTargetCommand);

  zerokernel::Kernel::ExecutionContract controlContract = {};
  controlContract.flags = zerokernel::Kernel::kContractRunImmediate |
                          zerokernel::Kernel::kContractDropIfLate;
  zerokernel::Kernel::TaskConfig controlConfig = {
      "LoopControl",
      controlTask,
      50,
      0,
      0,
      zerokernel::Kernel::kPriorityCritical,
      true,
      controlContract,
      zerokernel::Kernel::kCapControl};

  ZeroKernel.addTask(controlConfig);
  ZeroKernel.addTask("LoopCommand", commandTask, 200, 0, true);
  ZeroKernel.setTaskPriority("LoopCommand", zerokernel::Kernel::kPriorityHigh);
  ZeroKernel.addTask("LoopDiag", diagnosticsTask, 500, 0, true);
  ZeroKernel.setTaskPriority("LoopDiag", zerokernel::Kernel::kPriorityLow);
  ZeroKernel.addTask("LoopRecovery", recoveryTask, 25, 0, true);
  ZeroKernel.setTaskPriority("LoopRecovery", zerokernel::Kernel::kPriorityHigh);
  ZeroKernel.addTask("LoopReport", reportTask, 250, 0, true);

  resetWindow(millis());
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
