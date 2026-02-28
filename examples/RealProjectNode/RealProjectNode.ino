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
const unsigned long kLinkDropPeriodMs = 12000UL;
const unsigned long kLinkRestoreDelayMs = 1500UL;
const Kernel::TopicKey kWiFiStateTopic = Kernel::makeTopicKey("real.wifi");
const Kernel::TopicKey kTelemetryTopic = Kernel::makeTopicKey("real.mqtt.telemetry");

ZeroWiFiMaintainer g_wifiMaintainer;
ZeroHttpPump g_httpPump;
ZeroMqttPump g_mqttPump;

bool g_wifiConnected = false;
bool g_wifiReadyToConnect = false;
bool g_mqttConnected = false;
unsigned long g_wifiConnectCalls = 0;
unsigned long g_wifiForcedDropAtMs = 0;
unsigned long g_lastDropWindowIndex = 0;
unsigned long g_httpConnectAttempts = 0;
unsigned long g_httpRequestSeq = 0;
unsigned long g_httpWriteCalls = 0;
unsigned long g_mqttPublishCalls = 0;
unsigned long g_nextExpectedUs = 0;
unsigned long g_lagAccumUs = 0;
unsigned long g_maxLagUs = 0;
unsigned long g_sampleRuns = 0;
unsigned long g_fastMisses = 0;
unsigned long g_sensorValue = 1000;
unsigned long g_startedAtMs = 0;
unsigned long g_lastSummaryAtMs = 0;

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

void updateLinkSimulation(unsigned long nowMs) {
  const unsigned long windowIndex = (nowMs / kLinkDropPeriodMs);
  if (windowIndex > 0 && windowIndex != g_lastDropWindowIndex) {
    g_lastDropWindowIndex = windowIndex;
    g_wifiConnected = false;
    g_mqttConnected = false;
    g_wifiReadyToConnect = false;
    g_wifiForcedDropAtMs = nowMs;
  }
}

bool isWiFiConnected() {
  return g_wifiConnected;
}

void connectWiFi() {
  const unsigned long nowMs = millis();
  ++g_wifiConnectCalls;

  if (!g_wifiReadyToConnect) {
    g_wifiReadyToConnect = true;
    return;
  }

  if (g_wifiForcedDropAtMs != 0 && (nowMs - g_wifiForcedDropAtMs) < kLinkRestoreDelayMs) {
    return;
  }

  g_wifiConnected = true;
  g_mqttConnected = true;
  g_wifiForcedDropAtMs = 0;
}

void disconnectWiFi() {
  g_wifiConnected = false;
  g_mqttConnected = false;
  g_wifiReadyToConnect = false;
  g_wifiForcedDropAtMs = millis();
}

ZeroHttpPump::StepResult httpConnectStep(const ZeroHttpPump::Request&, void*) {
  ++g_httpConnectAttempts;
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpWriteStep(const ZeroHttpPump::Request&, void*) {
  ++g_httpWriteCalls;
  if ((g_httpWriteCalls % 7UL) == 0) {
    return ZeroHttpPump::kStepFailed;
  }

  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpReadStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

bool mqttLinkProbe() {
  return g_mqttConnected;
}

bool mqttConnectStep(void*) {
  g_mqttConnected = g_wifiConnected;
  return g_mqttConnected;
}

void mqttLoopStep(void*) {}

bool mqttPublishStep(Kernel::TopicKey, const Kernel::EventValue&, void*) {
  ++g_mqttPublishCalls;
  return (g_mqttPublishCalls % 9UL) != 0;
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
  ++g_sensorValue;
  g_nextExpectedUs += kSamplePeriodUs;
  if (nowUs > g_nextExpectedUs + kSamplePeriodUs) {
    g_nextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void queueTask() {
  static char payload[64];
  const int written =
      snprintf(payload, sizeof(payload), "{\"seq\":%lu,\"sensor\":%lu}",
               g_httpRequestSeq++, g_sensorValue);
  if (written > 0) {
    ZeroHttpPump::Request request;
    request.path = "/api/data";
    request.contentType = "application/json";
    request.body = payload;
    request.bodyLength = static_cast<uint16_t>(written);
    g_httpPump.enqueue(request);
  }

  g_mqttPump.enqueue(kTelemetryTopic, Kernel::EventValue::fromUnsigned(g_sensorValue), 1);
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - g_lastSummaryAtMs) < kSummaryPeriodMs) {
    return;
  }

  g_lastSummaryAtMs = nowMs;
  updateLinkSimulation(nowMs);

  const unsigned long windowMs = nowMs - g_startedAtMs;
  const unsigned long avgLagUs = g_sampleRuns == 0 ? 0 : g_lagAccumUs / g_sampleRuns;
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot httpMetrics =
      g_httpPump.metrics().snapshot();
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot mqttMetrics =
      g_mqttPump.metrics().snapshot();

  Serial.print("REAL_PROJECT_NODE window_ms=");
  Serial.print(windowMs);
  Serial.print(" sample_runs=");
  Serial.print(g_sampleRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(g_maxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(g_fastMisses);
  Serial.print(" link_up=");
  Serial.print(g_wifiConnected ? 1 : 0);
  Serial.print(" wifi_attempts=");
  Serial.print(g_wifiMaintainer.connectAttempts());
  Serial.print(" reconnects=");
  Serial.print(g_wifiMaintainer.reconnectTransitions());
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
  Serial.print(g_httpPump.queuedCount());
  Serial.print(" mqtt_queue=");
  Serial.println(g_mqttPump.queuedCount());
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
  wifiConfig.pollIntervalMs = 250;
  wifiConfig.retryBaseMs = 500;
  wifiConfig.retryMaxMs = 1000;
  wifiConfig.stateTopicKey = kWiFiStateTopic;
  g_wifiMaintainer.begin(ZeroKernel, isWiFiConnected, connectWiFi, disconnectWiFi, wifiConfig);

  ZeroHttpPump::Config httpConfig;
  httpConfig.pollIntervalMs = 100;
  httpConfig.retryBaseMs = 300;
  httpConfig.retryMaxMs = 600;
  httpConfig.maxRetries = 1;
  g_httpPump.begin(ZeroKernel,
                   httpConnectStep,
                   httpWriteStep,
                   httpReadStep,
                   NULL,
                   httpConfig);

  ZeroMqttPump::Config mqttConfig;
  mqttConfig.pollIntervalMs = 100;
  mqttConfig.retryBaseMs = 250;
  mqttConfig.retryMaxMs = 500;
  mqttConfig.maxRetries = 1;
  mqttConfig.stateTopicKey = 0;
  g_mqttPump.begin(ZeroKernel,
                   mqttLinkProbe,
                   mqttConnectStep,
                   mqttLoopStep,
                   mqttPublishStep,
                   mqttConfig);

  ZeroKernel.addTask("Sample", sampleTask, 100, 0);
  ZeroKernel.addTask("Queue", queueTask, 300, 0);
  ZeroKernel.addTask("Report", reportTask, 1000, 0);

  const unsigned long startedAtMs = millis();
  g_startedAtMs = startedAtMs;
  g_lastSummaryAtMs = startedAtMs;
}

void loop() {
  updateLinkSimulation(millis());
  zerokernel::adapters::powerSaveTick(ZeroKernel);
  g_wifiMaintainer.tick();
  g_httpPump.tick();
  g_mqttPump.tick();
}
