#include <Arduino.h>

#include <ZeroKernel.h>
#include <adapters/Esp8266Watchdog.h>

static const unsigned long kFastIntervalMs = 10;
static const unsigned long kFloodIntervalMs = 50;
static const unsigned long kReporterIntervalMs = 5000;

unsigned long g_fastRuns = 0;
unsigned long g_overruns = 0;
unsigned long g_drops = 0;

static unsigned long boardMillis() {
  return millis();
}

void writeSerialLine(const char* line) {
  Serial.println(line);
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  if (signal.type == zerokernel::Kernel::kSignalExecutionOverrun) {
    ++g_overruns;
  }

  if (signal.type == zerokernel::Kernel::kSignalEventDrop) {
    ++g_drops;
  }
}

void fastTask() {
  ++g_fastRuns;
  delayMicroseconds(800);
}

void floodQueueTask() {
  for (uint8_t i = 0; i < 24; ++i) {
    ZeroKernel.publishDeferredTyped("stress.queue",
                                    zerokernel::Kernel::EventValue::fromUnsigned(i));
  }
}

void overrunTask() {
  delayMicroseconds(3000);
}

void reportTask() {
  zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();

  Serial.print("LEVEL2 runs=");
  Serial.print(g_fastRuns);
  Serial.print(" overruns=");
  Serial.print(g_overruns);
  Serial.print(" queue_drop=");
  Serial.print(g_drops);
  Serial.print(" task_failures=");
  Serial.print(stats.taskFailures);
  Serial.print(" execution_overruns=");
  Serial.print(stats.executionOverruns);
  Serial.print(" deadline_miss=");
  Serial.println(stats.deadlineMisses);

  ZeroKernel.dumpTrace(writeSerialLine);
  ZeroKernel.clearTrace();
  g_fastRuns = 0;
  g_overruns = 0;
  g_drops = 0;
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);
  ZeroKernel.setSignalHandler(onSignal);
  ZeroKernel.setHardwareWatchdogBridge(zerokernel::adapters::esp8266WatchdogBridge());
  ZeroKernel.addTask("FastTask", fastTask, kFastIntervalMs, 2);
  ZeroKernel.addTask("FloodQueue", floodQueueTask, kFloodIntervalMs, 5);
  ZeroKernel.addTask("OverrunTask", overrunTask, 250, 1);
  ZeroKernel.addTask("Reporter", reportTask, kReporterIntervalMs, 20);
  ZeroKernel.setTaskPriority("FastTask", zerokernel::Kernel::kPriorityCritical);
  ZeroKernel.setTaskPriority("Reporter", zerokernel::Kernel::kPriorityLow);

  Serial.println("Wemos ZeroKernel level-2 stress test");
}

void loop() {
  ZeroKernel.tick();
}
