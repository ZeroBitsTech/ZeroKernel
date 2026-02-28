#include <Arduino.h>

#include <ZeroKernel.h>

static const unsigned long kFastIntervalMs = 10;
static const unsigned long kChunkIntervalMs = 4;
static const unsigned long kChunksPerSlowCycle = 10;
static const unsigned long kWindowMs = 5000;
static const unsigned long kFastIntervalUs = kFastIntervalMs * 1000UL;
static const unsigned long kMissThresholdUs = 1500;

unsigned long g_windowStartMs = 0;
unsigned long g_fastExpectedUs = 0;
unsigned long g_fastRuns = 0;
unsigned long g_slowCycles = 0;
unsigned long g_fastLagTotalUs = 0;
unsigned long g_fastLagMaxUs = 0;
unsigned long g_fastMissCount = 0;
unsigned long g_workChunks = 0;

static unsigned long boardMillis() {
  return millis();
}

void runCpuLoad(unsigned int cycles) {
  volatile unsigned long sink = 0;
  for (unsigned int i = 0; i < cycles; ++i) {
    sink += (i * 7UL) ^ (sink >> 1);
  }
}

void runFastTask() {
  const unsigned long nowUs = micros();
  const unsigned long lagUs = nowUs > g_fastExpectedUs ? (nowUs - g_fastExpectedUs) : 0;

  ++g_fastRuns;
  g_fastLagTotalUs += lagUs;
  if (lagUs > g_fastLagMaxUs) {
    g_fastLagMaxUs = lagUs;
  }
  if (lagUs > kMissThresholdUs) {
    ++g_fastMissCount;
  }
  g_fastExpectedUs += kFastIntervalUs;

  runCpuLoad(4500);
}

void runChunkedSlowTask() {
  ++g_workChunks;
  runCpuLoad(800);

  if (g_workChunks >= kChunksPerSlowCycle) {
    g_workChunks = 0;
    ++g_slowCycles;
  }
}

void printWindowSummary() {
  const unsigned long nowMs = millis();
  const unsigned long elapsedMs = nowMs - g_windowStartMs;
  const unsigned long avgFastLagUs =
      g_fastRuns > 0 ? (g_fastLagTotalUs / g_fastRuns) : 0;
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  const uint8_t traceCaptured = ZeroKernel.traceCount();

  Serial.print("ZEROKERNEL window_ms=");
  Serial.print(elapsedMs);
  Serial.print(" fast_runs=");
  Serial.print(g_fastRuns);
  Serial.print(" slow_runs=");
  Serial.print(g_slowCycles);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgFastLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(g_fastLagMaxUs);
  Serial.print(" fast_miss=");
  Serial.print(g_fastMissCount);
  Serial.print(" deadline_miss=");
  Serial.print(stats.deadlineMisses);
  Serial.print(" queue_drop=");
  Serial.print(stats.queuedEventsDropped);
  Serial.print(" scheduler_loops=");
  Serial.print(stats.schedulerLoops);
  Serial.print(" trace_count=");
  Serial.println(traceCaptured);

  g_windowStartMs = nowMs;
  g_fastRuns = 0;
  g_slowCycles = 0;
  g_fastLagTotalUs = 0;
  g_fastLagMaxUs = 0;
  g_fastMissCount = 0;
  ZeroKernel.clearTrace();
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);
  ZeroKernel.addTask("FastTask", runFastTask, kFastIntervalMs, 5);
  ZeroKernel.addTask("ChunkedWork", runChunkedSlowTask, kChunkIntervalMs, 2);
  ZeroKernel.addTask("Reporter", printWindowSummary, kWindowMs, 20);
  ZeroKernel.setTaskPriority("FastTask", zerokernel::Kernel::kPriorityCritical);
  ZeroKernel.setTaskPriority("ChunkedWork", zerokernel::Kernel::kPriorityNormal);
  ZeroKernel.setTaskPriority("Reporter", zerokernel::Kernel::kPriorityLow);
  ZeroKernel.setTaskHeartbeatTimeout("FastTask", 20);
  ZeroKernel.setTaskHeartbeatTimeout("ChunkedWork", 10);
  g_windowStartMs = millis();
  g_fastExpectedUs = micros() + kFastIntervalUs;

  Serial.println("Wemos ZeroKernel stress test");
}

void loop() {
  ZeroKernel.tick();
}
