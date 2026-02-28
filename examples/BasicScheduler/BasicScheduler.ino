#include <ZeroKernel.h>

static unsigned long boardMillis() {
  return millis();
}

void publishSensorSample() {
  static long sample = 24;
  ++sample;
  ZeroKernel.publish("temperature", sample);
}

void printTemperature(const char* topic, long value) {
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(value);
}

void setup() {
  Serial.begin(115200);

  ZeroKernel.begin(boardMillis);

  const zerokernel::Kernel::Identity& kernelIdentity = ZeroKernel.identity();
  Serial.println(kernelIdentity.name);
  Serial.println(kernelIdentity.tagline);
  Serial.println(kernelIdentity.vendor);

  ZeroKernel.subscribe("temperature", printTemperature);
  ZeroKernel.addTask("SensorReader", publishSensorSample, 500, 10);
  ZeroKernel.heartbeatTask("SensorReader");
}

void loop() {
  ZeroKernel.tick();
}
