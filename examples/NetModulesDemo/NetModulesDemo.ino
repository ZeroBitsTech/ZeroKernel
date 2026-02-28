#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>
#include <modules/net/ZeroHttpPump.h>
#include <modules/net/ZeroMqttPump.h>
#include <modules/net/ZeroWiFiMaintainer.h>

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroHttpPump;
using zerokernel::modules::net::ZeroMqttPump;
using zerokernel::modules::net::ZeroWiFiMaintainer;

namespace {

const unsigned long kSamplePeriodUs = 100000UL;
const unsigned long kSummaryPeriodMs = 5000UL;
const Kernel::TopicKey kWiFiStateTopic = Kernel::makeTopicKey("wifi.link");

ZeroWiFiMaintainer g_wifiMaintainer;
ZeroHttpPump g_httpPump;
ZeroMqttPump g_mqttPump;

bool g_wifiConnected = false;
bool g_failNextHttp = true;
bool g_mqttConnected = false;
bool g_failNextMqtt = true;
unsigned long g_requestId = 0;
unsigned long g_nextExpectedUs = 0;
unsigned long g_lagAccumUs = 0;
unsigned long g_maxLagUs = 0;
unsigned long g_sampleRuns = 0;
unsigned long g_fastMisses = 0;
unsigned long g_startedAtMs = 0;
unsigned long g_lastSummaryAtMs = 0;

unsigned long boardMillis() {
  return millis();
}

bool isWiFiConnected() {
  return g_wifiConnected;
}

void connectWiFi() {
  g_wifiConnected = true;
  g_mqttConnected = true;
}

void disconnectWiFi() {
  g_wifiConnected = false;
  g_mqttConnected = false;
}

ZeroHttpPump::StepResult connectStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult writeStep(const ZeroHttpPump::Request&, void*) {
  if (g_failNextHttp) {
    g_failNextHttp = false;
    return ZeroHttpPump::kStepFailed;
  }

  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult readStep(const ZeroHttpPump::Request&, void*) {
  g_failNextHttp = true;
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult closeStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

bool mqttLinkProbe() {
  return g_mqttConnected;
}

bool mqttConnectStep(void*) {
  g_mqttConnected = true;
  return true;
}

void mqttLoopStep(void*) {}

bool mqttPublishStep(Kernel::TopicKey, const Kernel::EventValue&, void*) {
  if (g_failNextMqtt) {
    g_failNextMqtt = false;
    return false;
  }

  g_failNextMqtt = true;
  return true;
}

void sampleTask() {
  const unsigned long nowUs = micros();
  if (g_nextExpectedUs == 0) {
    g_nextExpectedUs = nowUs;
  }

  const unsigned long lagUs = nowUs > g_nextExpectedUs ? nowUs - g_nextExpectedUs : 0;
  g_lagAccumUs += lagUs;
  if (lagUs > g_maxLagUs) {
    g_maxLagUs = lagUs;
  }
  if (lagUs > 1500UL) {
    ++g_fastMisses;
  }
  ++g_sampleRuns;
  g_nextExpectedUs += kSamplePeriodUs;
  if (nowUs > g_nextExpectedUs + kSamplePeriodUs) {
    g_nextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void queueTask() {
  static char payload[48];
  const int written = snprintf(payload, sizeof(payload), "{\"seq\":%lu}", g_requestId++);
  if (written > 0) {
    ZeroHttpPump::Request request;
    request.path = "/api/data";
    request.contentType = "application/json";
    request.body = payload;
    request.bodyLength = static_cast<uint16_t>(written);
    g_httpPump.enqueue(request);
  }

  g_mqttPump.enqueue(Kernel::makeTopicKey("mqtt.out"),
                     Kernel::EventValue::fromUnsigned(g_requestId),
                     1);
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - g_lastSummaryAtMs) < kSummaryPeriodMs) {
    return;
  }

  g_lastSummaryAtMs = nowMs;
  const unsigned long windowMs = nowMs - g_startedAtMs;
  const unsigned long avgLagUs = g_sampleRuns == 0 ? 0 : g_lagAccumUs / g_sampleRuns;
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot httpMetrics =
      g_httpPump.metrics().snapshot();
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot mqttMetrics =
      g_mqttPump.metrics().snapshot();

  Serial.print("ZEROKERNEL_NETMODULES window_ms=");
  Serial.print(windowMs);
  Serial.print(" sample_runs=");
  Serial.print(g_sampleRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(g_maxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(g_fastMisses);
  Serial.print(" wifi_attempts=");
  Serial.print(g_wifiMaintainer.connectAttempts());
  Serial.print(" wifi_reconnects=");
  Serial.print(g_wifiMaintainer.reconnectTransitions());
  Serial.print(" http_ok=");
  Serial.print(httpMetrics.sendSuccesses);
  Serial.print(" http_fail=");
  Serial.print(httpMetrics.sendFailures);
  Serial.print(" mqtt_ok=");
  Serial.print(mqttMetrics.sendSuccesses);
  Serial.print(" mqtt_fail=");
  Serial.print(mqttMetrics.sendFailures);
  Serial.print(" queue_max=");
  Serial.println(httpMetrics.maxQueueDepth);
}

void onWiFiState(const char*, const Kernel::EventValue&) {}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleSleep);
  ZeroKernel.subscribeTypedFast(kWiFiStateTopic, onWiFiState);

  ZeroWiFiMaintainer::Config wifiConfig;
  wifiConfig.pollIntervalMs = 300;
  wifiConfig.retryBaseMs = 300;
  wifiConfig.retryMaxMs = 300;
  wifiConfig.stateTopicKey = kWiFiStateTopic;
  g_wifiMaintainer.begin(ZeroKernel, isWiFiConnected, connectWiFi, disconnectWiFi, wifiConfig);

  ZeroHttpPump::Config httpConfig;
  httpConfig.pollIntervalMs = 200;
  httpConfig.retryBaseMs = 200;
  httpConfig.retryMaxMs = 200;
  httpConfig.maxRetries = 0;
  g_httpPump.begin(ZeroKernel, connectStep, writeStep, readStep, closeStep, httpConfig);

  ZeroMqttPump::Config mqttConfig;
  mqttConfig.pollIntervalMs = 150;
  mqttConfig.retryBaseMs = 150;
  mqttConfig.retryMaxMs = 150;
  mqttConfig.maxRetries = 0;
  g_mqttPump.begin(ZeroKernel,
                   mqttLinkProbe,
                   mqttConnectStep,
                   mqttLoopStep,
                   mqttPublishStep,
                   mqttConfig);

  ZeroKernel.addTask("Sample", sampleTask, 100, 0);
  ZeroKernel.addTask("Queue", queueTask, 250, 0);
  ZeroKernel.addTask("Report", reportTask, 1000, 0);

  const unsigned long startedAtMs = millis();
  g_startedAtMs = startedAtMs;
  g_lastSummaryAtMs = startedAtMs;
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
  g_wifiMaintainer.tick();
  g_httpPump.tick();
  g_mqttPump.tick();
}
