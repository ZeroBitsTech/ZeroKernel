#include <ZeroKernel.h>

static unsigned long boardMillis() {
  return millis();
}

void emitStatus() {
  ZeroKernel.publishTyped("status.online",
                          zerokernel::Kernel::EventValue::fromBool(true));
  ZeroKernel.publishDeferredTyped("counter.total",
                                  zerokernel::Kernel::EventValue::fromUnsigned(42));
}

void onTypedEvent(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print(topic);
  Serial.print(" => ");

  if (value.type == zerokernel::Kernel::kEventBool) {
    Serial.println(value.boolValue ? "true" : "false");
    return;
  }

  if (value.type == zerokernel::Kernel::kEventUnsigned) {
    Serial.println(value.unsignedValue);
    return;
  }

  Serial.println("unsupported");
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);
  ZeroKernel.subscribeTyped("status.online", onTypedEvent);
  ZeroKernel.subscribeTyped("counter.total", onTypedEvent);
  ZeroKernel.addTask("TypedEmitter", emitStatus, 1000, 5);
}

void loop() {
  ZeroKernel.tick();
}
