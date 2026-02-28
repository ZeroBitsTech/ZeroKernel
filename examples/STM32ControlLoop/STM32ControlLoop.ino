#include <ZeroKernel.h>
#include <adapters/Stm32Clock.h>

void controlTick() {
  static bool outputState = false;
  outputState = !outputState;
  ZeroKernel.publishTyped("control.state",
                          zerokernel::Kernel::EventValue::fromBool(outputState));
}

void onControlState(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value.boolValue ? "active" : "idle");
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(zerokernel::adapters::stm32HalClock);
  ZeroKernel.subscribeTyped("control.state", onControlState);
  ZeroKernel.addTask("ControlLoop", controlTick, 20, 2);
  ZeroKernel.setTaskPriority("ControlLoop", zerokernel::Kernel::kPriorityCritical);
}

void loop() {
  ZeroKernel.tick();
}
