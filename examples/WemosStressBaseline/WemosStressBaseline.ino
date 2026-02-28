#include <Arduino.h>

static const unsigned long kFastIntervalMs = 10;
static const unsigned long kSlowIntervalMs = 40;
static const unsigned long kWindowMs = 5000;
static const unsigned long kFastIntervalUs = kFastIntervalMs * 1000UL;
static const unsigned long kSlowIntervalUs = kSlowIntervalMs * 1000UL;
static const unsigned long kMissThresholdUs = 1500;

unsigned long g_nextFastMs = 0;
unsigned long g_nextSlowMs = 0;
unsigned long g_nextFastUs = 0;
unsigned long g_nextSlowUs = 0;
unsigned long g_windowStartMs = 0;
unsigned long g_loopCount = 0;
unsigned long g_fastRuns = 0;
unsigned long g_slowRuns = 0;
unsigned long g_fastLagTotalUs = 0;
unsigned long g_fastLagMaxUs = 0;
unsigned long g_slowLagMaxUs = 0;
unsigned long g_fastMissCount = 0;

void runCpuLoad(unsigned int cycles) {
  volatile unsigned long sink = 0;
  for (unsigned int i = 0; i < cycles; ++i) {
    sink += (i * 7UL) ^ (sink >> 1);
  }
}

void runFastTask() {
  const unsigned long nowUs = micros();
  const unsigned long lagUs = nowUs > g_nextFastUs ? (nowUs - g_nextFastUs) : 0;

  ++g_fastRuns;
  g_fastLagTotalUs += lagUs;
  if (lagUs > g_fastLagMaxUs) {
    g_fastLagMaxUs = lagUs;
  }
  if (lagUs > kMissThresholdUs) {
    ++g_fastMissCount;
  }

  g_nextFastUs += kFastIntervalUs;
  g_nextFastMs += kFastIntervalMs;
  runCpuLoad(4500);
}

void runSlowTask() {
  const unsigned long nowUs = micros();
  const unsigned long lagUs = nowUs > g_nextSlowUs ? (nowUs - g_nextSlowUs) : 0;

  ++g_slowRuns;
  if (lagUs > g_slowLagMaxUs) {
    g_slowLagMaxUs = lagUs;
  }

  g_nextSlowUs += kSlowIntervalUs;
  g_nextSlowMs += kSlowIntervalMs;

  // This is the typical blocking pattern ZeroKernel is meant to replace.
  delay(18);
  runCpuLoad(8000);
}

void printWindowSummary(unsigned long nowMs) {
  const unsigned long elapsedMs = nowMs - g_windowStartMs;
  const unsigned long avgFastLagUs =
      g_fastRuns > 0 ? (g_fastLagTotalUs / g_fastRuns) : 0;

  Serial.print("BASELINE window_ms=");
  Serial.print(elapsedMs);
  Serial.print(" fast_runs=");
  Serial.print(g_fastRuns);
  Serial.print(" slow_runs=");
  Serial.print(g_slowRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgFastLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(g_fastLagMaxUs);
  Serial.print(" slow_max_lag_us=");
  Serial.print(g_slowLagMaxUs);
  Serial.print(" fast_miss=");
  Serial.print(g_fastMissCount);
  Serial.print(" loop_count=");
  Serial.println(g_loopCount);

  g_windowStartMs = nowMs;
  g_loopCount = 0;
  g_fastRuns = 0;
  g_slowRuns = 0;
  g_fastLagTotalUs = 0;
  g_fastLagMaxUs = 0;
  g_slowLagMaxUs = 0;
  g_fastMissCount = 0;
}

void setup() {
  Serial.begin(115200);

  const unsigned long nowMs = millis();
  const unsigned long nowUs = micros();

  g_nextFastMs = nowMs + kFastIntervalMs;
  g_nextSlowMs = nowMs + kSlowIntervalMs;
  g_nextFastUs = nowUs + kFastIntervalUs;
  g_nextSlowUs = nowUs + kSlowIntervalUs;
  g_windowStartMs = nowMs;

  Serial.println("Wemos baseline stress test");
}

void loop() {
  ++g_loopCount;
  const unsigned long nowMs = millis();

  if (static_cast<long>(nowMs - g_nextFastMs) >= 0) {
    runFastTask();
  }

  if (static_cast<long>(nowMs - g_nextSlowMs) >= 0) {
    runSlowTask();
  }

  if ((nowMs - g_windowStartMs) >= kWindowMs) {
    printWindowSummary(nowMs);
  }
}
