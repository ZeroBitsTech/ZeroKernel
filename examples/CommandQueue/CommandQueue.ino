#include <Arduino.h>

#include <ZeroKernel.h>

static const unsigned long kCommandIntervalMs = 1000;
static const unsigned long kEventIntervalMs = 1500;
static const unsigned long kReportIntervalMs = 3000;

unsigned long g_commandSequence = 0;
unsigned long g_eventSequence = 0;

static unsigned long boardMillis() {
  return millis();
}

void onCommand(const char* command, const zerokernel::Kernel::EventValue& value) {
  Serial.print("CMD ");
  Serial.print(command);
  Serial.print(" => ");
  if (value.type == zerokernel::Kernel::kEventUnsigned) {
    Serial.println(value.unsignedValue);
  } else {
    Serial.println(value.longValue);
  }
}

void onEvent(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print("EVENT ");
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value.unsignedValue);
}

void queueCommandTask() {
  ++g_commandSequence;
  ZeroKernel.enqueueCommandTyped("cmd.sample",
                                 zerokernel::Kernel::EventValue::fromUnsigned(
                                     g_commandSequence));
}

void publishEventTask() {
  ++g_eventSequence;
  ZeroKernel.publishDeferredTyped("event.sample",
                                  zerokernel::Kernel::EventValue::fromUnsigned(
                                      g_eventSequence));
}

void reportTask() {
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  Serial.print("STATS commands=");
  Serial.print(stats.commandsQueued);
  Serial.print(" delivered=");
  Serial.print(stats.commandsDelivered);
  Serial.print(" events=");
  Serial.print(stats.eventsPublished);
  Serial.print(" event_delivered=");
  Serial.println(stats.eventsDelivered);
}

void setup() {
  Serial.begin(115200);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.registerCommand("cmd.sample", onCommand);
  ZeroKernel.subscribeTyped("event.sample", onEvent);
  ZeroKernel.addTask("QueueCommand", queueCommandTask, kCommandIntervalMs, 5);
  ZeroKernel.addTask("PublishEvent", publishEventTask, kEventIntervalMs, 5);
  ZeroKernel.addTask("Reporter", reportTask, kReportIntervalMs, 5);
  ZeroKernel.setTaskPriority("QueueCommand", zerokernel::Kernel::kPriorityHigh);

  Serial.println("ZeroKernel command queue example");
}

void loop() {
  ZeroKernel.tick();
}
