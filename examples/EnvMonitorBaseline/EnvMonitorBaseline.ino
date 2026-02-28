#include <Arduino.h>

namespace {

const unsigned long kSamplePeriodUs = 100000UL;
const unsigned long kFilterPeriodMs = 200UL;
const unsigned long kAlarmPeriodMs = 150UL;
const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kFastMissThresholdUs = 1500UL;

unsigned long gNextExpectedUs = 0;
unsigned long gLagAccumUs = 0;
unsigned long gMaxLagUs = 0;
unsigned long gFastMisses = 0;
unsigned long gSampleRuns = 0;
unsigned long gFilterRuns = 0;
unsigned long gAlarmTrips = 0;
unsigned long gLastSampleAtUs = 0;
unsigned long gLastFilterAtMs = 0;
unsigned long gLastAlarmAtMs = 0;
unsigned long gWindowStartedAtMs = 0;
unsigned long gSensorValue = 420;
unsigned long gFilteredValue = 420;

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

void runFilter() {
  ++gFilterRuns;
  busyWaitUs(1800UL);
  gFilteredValue = ((gFilteredValue * 3UL) + gSensorValue) / 4UL;
}

void runAlarm() {
  if (gFilteredValue > 470UL) {
    ++gAlarmTrips;
  }
}

void runSample(unsigned long nowUs) {
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
  gNextExpectedUs += kSamplePeriodUs;
  if (nowUs > (gNextExpectedUs + kSamplePeriodUs)) {
    gNextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void maybeReport(unsigned long nowMs) {
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  const unsigned long avgLagUs = gSampleRuns == 0 ? 0 : (gLagAccumUs / gSampleRuns);
  Serial.print("BASELINE_ENVMONITOR window_ms=");
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

  const unsigned long nowMs = millis();
  gLastSampleAtUs = micros();
  gLastFilterAtMs = nowMs;
  gLastAlarmAtMs = nowMs;
  resetWindow(nowMs);
}

void loop() {
  const unsigned long nowMs = millis();
  const unsigned long nowUs = micros();

  if ((nowMs - gLastFilterAtMs) >= kFilterPeriodMs) {
    gLastFilterAtMs += kFilterPeriodMs;
    runFilter();
  }

  if ((nowMs - gLastAlarmAtMs) >= kAlarmPeriodMs) {
    gLastAlarmAtMs += kAlarmPeriodMs;
    runAlarm();
  }

  if ((nowUs - gLastSampleAtUs) >= kSamplePeriodUs) {
    gLastSampleAtUs += kSamplePeriodUs;
    runSample(nowUs);
  }

  maybeReport(nowMs);
}
