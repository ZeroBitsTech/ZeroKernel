#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

static unsigned long boardMillis() {
  return millis();
}

void emitHealth() {
  zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  ZeroKernel.publishDeferredTyped("kernel.health",
                                  zerokernel::Kernel::EventValue::fromUnsigned(
                                      stats.taskExecutions));
}

void onHealth(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value.unsignedValue);
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  Serial.print("signal ");
  Serial.print(signal.type);
  Serial.print(" ");
  Serial.print(signal.label);
  Serial.print(" ");
  Serial.println(signal.value);
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);
  ZeroKernel.setSignalHandler(onSignal);
  ZeroKernel.subscribeTyped("kernel.health", onHealth);
  ZeroKernel.addTask("HealthEmitter", emitHealth, 1000, 5);
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
