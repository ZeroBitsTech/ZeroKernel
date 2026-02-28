#include <ZeroKernel.h>

static unsigned long boardMillis() {
  return millis();
}

void sampleSensors() {
  static long reading = 200;
  ++reading;
  ZeroKernel.publishDeferred("telemetry.temperature", reading);
}

void printTelemetry(const char* topic, long value) {
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value);
}

void reportKernelHealth() {
  zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();

  Serial.print("loops=");
  Serial.print(stats.schedulerLoops);
  Serial.print(" runs=");
  Serial.print(stats.taskExecutions);
  Serial.print(" failures=");
  Serial.print(stats.taskFailures);
  Serial.print(" watchdog=");
  Serial.println(stats.watchdogTrips);
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);

  zerokernel::Kernel::WatchdogPolicy policy;
  policy.heartbeatTimeoutMs = 250;
  policy.maxConsecutiveFailures = 2;
  policy.autoRecovery = true;
  ZeroKernel.setWatchdogPolicy(policy);

  const zerokernel::Kernel::Identity& identity = ZeroKernel.identity();
  Serial.println(identity.name);
  Serial.println(identity.vendor);
  Serial.println(identity.tagline);

  ZeroKernel.subscribe("telemetry.temperature", printTelemetry);
  ZeroKernel.addTask("SensorSampler", sampleSensors, 500, 20);
  ZeroKernel.addTask("HealthReporter", reportKernelHealth, 1000, 25);
  ZeroKernel.setTaskHeartbeatTimeout("HealthReporter", 1500);
}

void loop() {
  ZeroKernel.tick();
}
