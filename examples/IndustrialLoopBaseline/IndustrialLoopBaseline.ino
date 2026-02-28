#include <Arduino.h>

namespace {

const unsigned long kControlPeriodUs = 50000UL;
const unsigned long kCommandPeriodMs = 200UL;
const unsigned long kDiagPeriodMs = 500UL;
const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kFastMissThresholdUs = 1500UL;

unsigned long gNextExpectedUs = 0;
unsigned long gLagAccumUs = 0;
unsigned long gMaxLagUs = 0;
unsigned long gFastMisses = 0;
unsigned long gControlRuns = 0;
unsigned long gCommandUpdates = 0;
unsigned long gDiagRuns = 0;
unsigned long gFaultWindows = 0;
unsigned long gLastControlAtUs = 0;
unsigned long gLastCommandAtMs = 0;
unsigned long gLastDiagAtMs = 0;
unsigned long gWindowStartedAtMs = 0;
long gTarget = 100;
long gMeasured = 90;

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
  gFaultWindows = 0;
}

void runDiagnostics() {
  ++gDiagRuns;
  if ((gDiagRuns % 6UL) == 0) {
    busyWaitUs(4000UL);
    ++gFaultWindows;
  } else {
    busyWaitUs(2500UL);
  }
}

void runCommand() {
  ++gCommandUpdates;
  gTarget = (gTarget == 100) ? 125 : 100;
}

void runControl(unsigned long nowUs) {
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

  gNextExpectedUs += kControlPeriodUs;
  if (nowUs > (gNextExpectedUs + kControlPeriodUs)) {
    gNextExpectedUs = nowUs + kControlPeriodUs;
  }
}

void maybeReport(unsigned long nowMs) {
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  const unsigned long avgLagUs = gControlRuns == 0 ? 0 : (gLagAccumUs / gControlRuns);
  Serial.print("BASELINE_INDUSTRIAL window_ms=");
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
  Serial.print(" fault_windows=");
  Serial.println(gFaultWindows);

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  const unsigned long nowMs = millis();
  gLastControlAtUs = micros();
  gLastCommandAtMs = nowMs;
  gLastDiagAtMs = nowMs;
  resetWindow(nowMs);
}

void loop() {
  const unsigned long nowMs = millis();
  const unsigned long nowUs = micros();

  if ((nowMs - gLastDiagAtMs) >= kDiagPeriodMs) {
    gLastDiagAtMs += kDiagPeriodMs;
    runDiagnostics();
  }

  if ((nowMs - gLastCommandAtMs) >= kCommandPeriodMs) {
    gLastCommandAtMs += kCommandPeriodMs;
    runCommand();
  }

  if ((nowUs - gLastControlAtUs) >= kControlPeriodUs) {
    gLastControlAtUs += kControlPeriodUs;
    runControl(nowUs);
  }

  maybeReport(nowMs);
  yield();
}
