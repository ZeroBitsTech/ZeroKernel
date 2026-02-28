#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

namespace {

const zerokernel::Kernel::TopicKey kStatusTopic =
    zerokernel::Kernel::makeTopicKey("status.tick");
const zerokernel::Kernel::EventFlags kFlagTelemetryReady = 0x01U;
unsigned long g_tickCount = 0;

void printLine(const char* line) {
  Serial.println(line);
}

void handleTick(const char*, long value) {
  Serial.print("tick => ");
  Serial.println(value);
}

void flushTelemetry(const zerokernel::Kernel::EventValue& value) {
  if (value.type != zerokernel::Kernel::kEventUnsigned) {
    return;
  }

  Serial.print("work.telemetry => ");
  Serial.println(value.unsignedValue);
}

void telemetryTask() {
  ++g_tickCount;
  ZeroKernel.publishDeferredFast(
      kStatusTopic, static_cast<long>(g_tickCount));
  ZeroKernel.setFlags(kFlagTelemetryReady);

  if (ZeroKernel.takeFlags(kFlagTelemetryReady) != 0U) {
    ZeroKernel.scheduleWorkTyped(
        flushTelemetry,
        zerokernel::Kernel::EventValue::fromUnsigned(g_tickCount));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  ZeroKernel.begin(millis);
  ZeroKernel.subscribeFast(kStatusTopic, handleTick, "status.tick");
  ZeroKernel.addTask("Telemetry", telemetryTask, 1000, 0);

  const zerokernel::Kernel::Identity& identity = ZeroKernel.identity();
  Serial.print(identity.name);
  Serial.print(" by ");
  Serial.println(identity.vendor);
  ZeroKernel.dumpStats(printLine);
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
