#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>
#include <modules/net/ZeroMqttPump.h>

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroMqttPump;

namespace {

ZeroMqttPump g_mqttPump;
bool g_brokerConnected = false;
bool g_failNextPublish = true;
unsigned long g_publishValue = 0;
const Kernel::TopicKey kBrokerStateTopic = Kernel::makeTopicKey("mqtt.state");
const Kernel::TopicKey kPublishTopic = Kernel::makeTopicKey("mqtt.out");

unsigned long boardMillis() {
  return millis();
}

bool linkProbe() {
  return g_brokerConnected;
}

bool connectStep(void*) {
  g_brokerConnected = true;
  return true;
}

void loopStep(void*) {}

bool publishStep(Kernel::TopicKey, const Kernel::EventValue&, void*) {
  if (g_failNextPublish) {
    g_failNextPublish = false;
    return false;
  }

  return true;
}

void onStateEvent(const char*, const Kernel::EventValue& value) {
  if (value.type != Kernel::kEventBool) {
    return;
  }

  Serial.print("mqtt.state => ");
  Serial.println(value.boolValue ? "connected" : "disconnected");
}

void queuePublishTask() {
  g_mqttPump.enqueue(kPublishTopic, Kernel::EventValue::fromUnsigned(g_publishValue++), 1);
}

void reportTask() {
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot snapshot =
      g_mqttPump.metrics().snapshot();

  Serial.print("mqtt queue=");
  Serial.print(g_mqttPump.queuedCount());
  Serial.print(" sent_ok=");
  Serial.print(snapshot.sendSuccesses);
  Serial.print(" sent_fail=");
  Serial.print(snapshot.sendFailures);
  Serial.print(" loop_calls=");
  Serial.println(snapshot.loopCalls);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.subscribeTypedFast(kBrokerStateTopic, onStateEvent);

  ZeroMqttPump::Config config;
  config.pollIntervalMs = 50;
  config.retryBaseMs = 100;
  config.retryMaxMs = 500;
  config.maxRetries = 1;
  config.stateTopicKey = kBrokerStateTopic;

  g_mqttPump.begin(ZeroKernel, linkProbe, connectStep, loopStep, publishStep, config);

  ZeroKernel.addTask("MqttQueue", queuePublishTask, 1200, 0);
  ZeroKernel.addTask("MqttReport", reportTask, 1000, 0);
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
  g_mqttPump.tick();
}
