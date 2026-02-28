#include <ZeroKernel.h>
#include <adapters/Esp32Clock.h>

void sampleHub() {
  static unsigned long sampleId = 0;
  ++sampleId;
  ZeroKernel.publishDeferredTyped("hub.sample",
                                  zerokernel::Kernel::EventValue::fromUnsigned(sampleId));
}

void reportHub(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value.unsignedValue);
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(zerokernel::adapters::esp32MillisClock);
  ZeroKernel.subscribeTyped("hub.sample", reportHub);
  ZeroKernel.addTask("HubSampler", sampleHub, 250, 5);
  ZeroKernel.setTaskPriority("HubSampler", zerokernel::Kernel::kPriorityHigh);
}

void loop() {
  ZeroKernel.tick();
}
