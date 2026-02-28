#include <stdio.h>

#include "ZeroKernel.h"

static long g_temperature = 24;

void sampleTemperature() {
  ++g_temperature;
  ZeroKernel.publishDeferred("desktop.temperature", g_temperature);
}

void printTemperature(const char* topic, long value) {
  printf("%s=%ld\n", topic, value);
}

int main() {
  zerokernel::Kernel::WatchdogPolicy policy = {25, 2, true};
  ZeroKernel.setWatchdogPolicy(policy);
  ZeroKernel.subscribe("desktop.temperature", printTemperature);
  ZeroKernel.addTask("DesktopSensor", sampleTemperature, 100, 10);
  ZeroKernel.setTaskHeartbeatTimeout("DesktopSensor", 50);

  for (unsigned long nowMs = 0; nowMs <= 500; nowMs += 100) {
    ZeroKernel.tick(nowMs);
  }

  zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  printf("runs=%lu failures=%lu uptime=%lu\n",
         static_cast<unsigned long>(stats.taskExecutions),
         static_cast<unsigned long>(stats.taskFailures),
         static_cast<unsigned long>(stats.uptimeMs));

  return 0;
}
