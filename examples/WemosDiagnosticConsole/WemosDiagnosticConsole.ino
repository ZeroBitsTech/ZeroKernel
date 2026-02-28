#include <Arduino.h>

#include <ZeroKernel.h>
#include <adapters/Esp8266Watchdog.h>

static const unsigned long kPulseIntervalMs = 250;
static const unsigned long kReportIntervalMs = 3000;

unsigned long g_pulseCount = 0;

static unsigned long boardMillis() {
  return millis();
}

void writeSerialLine(const char* line) {
  Serial.println(line);
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  Serial.print("signal type=");
  Serial.print(signal.type);
  Serial.print(" label=");
  Serial.print(signal.label);
  Serial.print(" value=");
  Serial.println(signal.value);
}

void pulseTask() {
  ++g_pulseCount;
  ZeroKernel.heartbeatTask("Pulse");
  ZeroKernel.publishDeferred("diag.pulse", static_cast<long>(g_pulseCount));
}

void reportTask() {
  const zerokernel::Kernel::Identity& identity = ZeroKernel.identity();
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  const zerokernel::Kernel::TimingReport timing = ZeroKernel.getTimingReport();

  Serial.print("IDENTITY ");
  Serial.print(identity.name);
  Serial.print(" ");
  Serial.print(identity.vendor);
  Serial.print(" ");
  Serial.println(identity.tagline);

  Serial.print("STATUS uptime_ms=");
  Serial.print(stats.uptimeMs);
  Serial.print(" state=");
  Serial.print(ZeroKernel.state());
  Serial.print(" abi=");
  Serial.print(ZeroKernel.abiVersion());
  Serial.print(" task_exec=");
  Serial.print(stats.taskExecutions);
  Serial.print(" events_delivered=");
  Serial.print(stats.eventsDelivered);
  Serial.print(" watchdog_trips=");
  Serial.print(stats.watchdogTrips);
#if defined(ESP8266)
  Serial.print(" free_heap=");
  Serial.println(ESP.getFreeHeap());
#else
  Serial.println();
#endif

  Serial.print("TIMING tick_max_ms=");
  Serial.print(timing.worstTickDurationMs);
  Serial.print(" task_max_ms=");
  Serial.print(timing.worstTaskDurationMs);
  Serial.print(" lag_max_ms=");
  Serial.println(timing.worstLagMs);

  ZeroKernel.dumpTasks(writeSerialLine);
  ZeroKernel.dumpTrace(writeSerialLine);
  ZeroKernel.clearTrace();
}

void setup() {
  Serial.begin(115200);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdlePlatformHint);
  ZeroKernel.setHardwareWatchdogBridge(zerokernel::adapters::esp8266WatchdogBridge());
  ZeroKernel.setSignalHandler(onSignal);

  ZeroKernel.addTask("Pulse", pulseTask, kPulseIntervalMs, 2);
  ZeroKernel.addTask("Reporter", reportTask, kReportIntervalMs, 10);
  ZeroKernel.setTaskPriority("Pulse", zerokernel::Kernel::kPriorityHigh);
  ZeroKernel.setTaskPriority("Reporter", zerokernel::Kernel::kPriorityLow);

  Serial.println("ZeroKernel Wemos diagnostic console");
#if defined(ESP8266)
  Serial.print("Boot free heap=");
  Serial.println(ESP.getFreeHeap());
#endif
}

void loop() {
  ZeroKernel.tick();
}
