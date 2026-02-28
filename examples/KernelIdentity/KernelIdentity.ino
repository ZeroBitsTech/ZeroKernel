#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

static unsigned long boardMillis() {
  return millis();
}

void printIdentity() {
  const zerokernel::Kernel::Identity& identity = ZeroKernel.identity();

  Serial.println(identity.name);
  Serial.println(identity.vendor);
  Serial.println(identity.tagline);
  Serial.println(identity.site);
  Serial.println(identity.version);
  Serial.println("Runtime identity exported by ZeroKernel.");
}

void setup() {
  Serial.begin(115200);
  ZeroKernel.begin(boardMillis);
  printIdentity();
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
