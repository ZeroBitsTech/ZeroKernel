#include <ZeroKernel.h>
#include <adapters/RP2040Clock.h>

void reportLoop() {
  zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();

  Serial.print("loops=");
  Serial.print(stats.schedulerLoops);
  Serial.print(" idle=");
  Serial.print(stats.schedulerIdleLoops);
  Serial.print(" misses=");
  Serial.println(stats.deadlineMisses);
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(zerokernel::adapters::rp2040MillisClock);
  ZeroKernel.addTask("LoopReporter", reportLoop, 1000, 5);
}

void loop() {
  ZeroKernel.tick();
}
