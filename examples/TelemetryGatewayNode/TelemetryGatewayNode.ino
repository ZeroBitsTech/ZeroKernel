#include <ZeroKernel.h>
#include <modules/net/ZeroHttpPump.h>
#include <modules/net/ZeroMqttPump.h>
#include <modules/net/ZeroWiFiMaintainer.h>

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroHttpPump;
using zerokernel::modules::net::ZeroMqttPump;
using zerokernel::modules::net::ZeroWiFiMaintainer;

namespace {

const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kLinkDropPeriodMs = 12000UL;
const unsigned long kLinkRestoreDelayMs = 1500UL;
const Kernel::TopicKey kWiFiStateTopic = Kernel::makeTopicKey("gateway.wifi");
const Kernel::TopicKey kTelemetryTopic = Kernel::makeTopicKey("gateway.mqtt.out");

ZeroWiFiMaintainer gWiFiMaintainer;
ZeroHttpPump gHttpPump;
ZeroMqttPump gMqttPump;

bool gLinkUp = false;
bool gReadyToConnect = false;
bool gMqttUp = false;
unsigned long gDropStartedAtMs = 0;
unsigned long gLastDropWindow = 0;
unsigned long gMessageSeq = 0;
unsigned long gWindowStartedAtMs = 0;
unsigned long gHttpWriteCalls = 0;
unsigned long gMqttPublishCalls = 0;

unsigned long boardMillis() {
  return millis();
}

unsigned long percentage(unsigned long ok, unsigned long fail) {
  const unsigned long total = ok + fail;
  if (total == 0) {
    return 100;
  }
  return (ok * 100UL) / total;
}

void resetWindow(unsigned long nowMs) {
  gWindowStartedAtMs = nowMs;
}

void updateLinkSimulation(unsigned long nowMs) {
  const unsigned long dropWindow = nowMs / kLinkDropPeriodMs;
  if (dropWindow > 0 && dropWindow != gLastDropWindow) {
    gLastDropWindow = dropWindow;
    gLinkUp = false;
    gReadyToConnect = false;
    gMqttUp = false;
    gDropStartedAtMs = nowMs;
  }
}

bool isLinkUp() {
  return gLinkUp;
}

void connectLink() {
  const unsigned long nowMs = millis();
  if (!gReadyToConnect) {
    gReadyToConnect = true;
    return;
  }

  if (gDropStartedAtMs != 0 && (nowMs - gDropStartedAtMs) < kLinkRestoreDelayMs) {
    return;
  }

  gLinkUp = true;
  gMqttUp = true;
  gDropStartedAtMs = 0;
}

void disconnectLink() {
  gLinkUp = false;
  gMqttUp = false;
  gReadyToConnect = false;
  gDropStartedAtMs = millis();
}

ZeroHttpPump::StepResult httpConnectStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpWriteStep(const ZeroHttpPump::Request&, void*) {
  ++gHttpWriteCalls;
  if ((gHttpWriteCalls % 7UL) == 0) {
    return ZeroHttpPump::kStepFailed;
  }
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpReadStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

bool mqttLinkProbe() {
  return gMqttUp;
}

bool mqttConnectStep(void*) {
  gMqttUp = gLinkUp;
  return gMqttUp;
}

void mqttLoopStep(void*) {}

bool mqttPublishStep(Kernel::TopicKey, const Kernel::EventValue&, void*) {
  ++gMqttPublishCalls;
  return (gMqttPublishCalls % 9UL) != 0;
}

void queueTelemetryTask() {
  static char payload[48];
  const int written =
      snprintf(payload, sizeof(payload), "{\"seq\":%lu}", gMessageSeq++);
  if (written > 0) {
    ZeroHttpPump::Request request;
    request.path = "/api/data";
    request.contentType = "application/json";
    request.body = payload;
    request.bodyLength = static_cast<uint16_t>(written);
    gHttpPump.enqueue(request);
  }

  gMqttPump.enqueue(kTelemetryTopic, Kernel::EventValue::fromUnsigned(gMessageSeq), 1);
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  updateLinkSimulation(nowMs);

  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot httpMetrics =
      gHttpPump.metrics().snapshot();
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot mqttMetrics =
      gMqttPump.metrics().snapshot();

  Serial.print("ZEROKERNEL_TELEMETRY_GATEWAY window_ms=");
  Serial.print(nowMs - gWindowStartedAtMs);
  Serial.print(" link_up=");
  Serial.print(gLinkUp ? 1 : 0);
  Serial.print(" wifi_attempts=");
  Serial.print(gWiFiMaintainer.connectAttempts());
  Serial.print(" reconnects=");
  Serial.print(gWiFiMaintainer.reconnectTransitions());
  Serial.print(" http_ok=");
  Serial.print(httpMetrics.sendSuccesses);
  Serial.print(" http_fail=");
  Serial.print(httpMetrics.sendFailures);
  Serial.print(" http_rate=");
  Serial.print(percentage(httpMetrics.sendSuccesses, httpMetrics.sendFailures));
  Serial.print(" mqtt_ok=");
  Serial.print(mqttMetrics.sendSuccesses);
  Serial.print(" mqtt_fail=");
  Serial.print(mqttMetrics.sendFailures);
  Serial.print(" mqtt_rate=");
  Serial.print(percentage(mqttMetrics.sendSuccesses, mqttMetrics.sendFailures));
  Serial.print(" http_queue=");
  Serial.print(gHttpPump.queuedCount());
  Serial.print(" mqtt_queue=");
  Serial.println(gMqttPump.queuedCount());

  resetWindow(nowMs);
}

void onWiFiState(const char*, const Kernel::EventValue&) {}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleYield);
  ZeroKernel.subscribeTypedFast(kWiFiStateTopic, onWiFiState);

  ZeroWiFiMaintainer::Config wifiConfig;
  wifiConfig.pollIntervalMs = 250;
  wifiConfig.retryBaseMs = 500;
  wifiConfig.retryMaxMs = 1000;
  wifiConfig.stateTopicKey = kWiFiStateTopic;
  gWiFiMaintainer.begin(ZeroKernel, isLinkUp, connectLink, disconnectLink, wifiConfig);

  ZeroHttpPump::Config httpConfig;
  httpConfig.pollIntervalMs = 50;
  httpConfig.retryBaseMs = 100;
  httpConfig.retryMaxMs = 200;
  httpConfig.maxRetries = 2;
  gHttpPump.begin(ZeroKernel, httpConnectStep, httpWriteStep, httpReadStep, NULL, httpConfig);

  ZeroMqttPump::Config mqttConfig;
  mqttConfig.pollIntervalMs = 50;
  mqttConfig.retryBaseMs = 100;
  mqttConfig.retryMaxMs = 200;
  mqttConfig.maxRetries = 2;
  gMqttPump.begin(ZeroKernel, mqttLinkProbe, mqttConnectStep, mqttLoopStep, mqttPublishStep, mqttConfig);

  ZeroKernel.addTask("GatewayQueue", queueTelemetryTask, 180, 0, true);
  ZeroKernel.addTask("GatewayReport", reportTask, 250, 0, true);
  resetWindow(millis());
}

void loop() {
  ZeroKernel.tick();
  gWiFiMaintainer.tick();
  gHttpPump.tick();
  gMqttPump.tick();
  yield();
}
