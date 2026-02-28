#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

namespace {

const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kFastMissThresholdUs = 1500UL;

unsigned long gNextExpectedUs = 0;
unsigned long gLagAccumUs = 0;
unsigned long gMaxLagUs = 0;
unsigned long gFastMisses = 0;
unsigned long gSampleRuns = 0;
unsigned long gFilterRuns = 0;
unsigned long gAlarmTrips = 0;
unsigned long gWindowStartedAtMs = 0;
unsigned long gSensorValue = 420;
unsigned long gFilteredValue = 420;

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
  gSampleRuns = 0;
  gFilterRuns = 0;
  gAlarmTrips = 0;
}

void sampleTask() {
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

  ++gSampleRuns;
  gSensorValue = 420UL + ((millis() / 23UL) % 90UL);
  gNextExpectedUs += 100000UL;
  if (nowUs > (gNextExpectedUs + 100000UL)) {
    gNextExpectedUs = nowUs + 100000UL;
  }
}

void filterTask() {
  ++gFilterRuns;
  busyWaitUs(1800UL);
  gFilteredValue = ((gFilteredValue * 3UL) + gSensorValue) / 4UL;
}

void alarmTask() {
  if (gFilteredValue > 470UL) {
    ++gAlarmTrips;
  }
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  const unsigned long avgLagUs = gSampleRuns == 0 ? 0 : (gLagAccumUs / gSampleRuns);
  Serial.print("ZEROKERNEL_ENVMONITOR window_ms=");
  Serial.print(nowMs - gWindowStartedAtMs);
  Serial.print(" sample_runs=");
  Serial.print(gSampleRuns);
  Serial.print(" filter_runs=");
  Serial.print(gFilterRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(gMaxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(gFastMisses);
  Serial.print(" alarm_trips=");
  Serial.println(gAlarmTrips);

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleSleep);

  zerokernel::Kernel::ExecutionContract sampleContract = {};
  sampleContract.flags = zerokernel::Kernel::kContractRunImmediate |
                         zerokernel::Kernel::kContractDropIfLate;
  zerokernel::Kernel::TaskConfig sampleConfig = {
      "EnvSample",
      sampleTask,
      100,
      0,
      0,
      zerokernel::Kernel::kPriorityCritical,
      true,
      sampleContract,
      zerokernel::Kernel::kCapNone};

  ZeroKernel.addTask(sampleConfig);
  ZeroKernel.addTask("EnvFilter", filterTask, 200, 0, true);
  ZeroKernel.setTaskPriority("EnvFilter", zerokernel::Kernel::kPriorityNormal);
  ZeroKernel.addTask("EnvAlarm", alarmTask, 150, 0, true);
  ZeroKernel.setTaskPriority("EnvAlarm", zerokernel::Kernel::kPriorityHigh);
  ZeroKernel.addTask("EnvReport", reportTask, 250, 0, true);

  resetWindow(millis());
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
