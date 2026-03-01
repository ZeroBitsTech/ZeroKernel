#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>
#include <modules/net/ZeroHttpPump.h>
#include <modules/net/ZeroMqttPump.h>
#include <modules/net/ZeroNetProfileEsp8266.h>
#include <modules/net/ZeroWiFiMaintainer.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#error "LiveNetworkNode requires ESP8266 or ESP32."
#endif

#include <PubSubClient.h>
#include "LocalSecrets.h"

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroHttpPump;
using zerokernel::modules::net::ZeroMqttPump;
using zerokernel::modules::net::ZeroNetProfileEsp8266;
using zerokernel::modules::net::ZeroTransportMetrics;
using zerokernel::modules::net::ZeroWiFiMaintainer;

namespace {

const unsigned long kSamplePeriodUs = 100000UL;
#if defined(ARDUINO_ARCH_ESP8266)
const unsigned long kSampleTaskIntervalMs = ZeroNetProfileEsp8266::kSampleTaskIntervalMs;
const unsigned long kHttpDispatchPeriodMs = ZeroNetProfileEsp8266::kHttpDispatchPeriodMs;
const unsigned long kMqttDispatchPeriodMs = ZeroNetProfileEsp8266::kMqttDispatchPeriodMs;
const unsigned long kHttpIoTimeoutMs = ZeroNetProfileEsp8266::kHttpIoTimeoutMs;
#else
const unsigned long kSampleTaskIntervalMs = 1UL;
const unsigned long kHttpDispatchPeriodMs = 500UL;
const unsigned long kMqttDispatchPeriodMs = 500UL;
const unsigned long kHttpIoTimeoutMs = 500UL;
#endif
const unsigned long kSummaryPeriodMs = 10000UL;
const unsigned long kMissThresholdUs = 1500UL;
const unsigned long kWiFiMaintStartDelayMs = 20UL;
const unsigned long kHttpPumpStartDelayMs = 40UL;
const unsigned long kMqttPumpStartDelayMs = 60UL;
const unsigned long kDispatchStartDelayMs = 80UL;
const unsigned long kReportStartDelayMs = 125UL;

const Kernel::TopicKey kWiFiStateTopic = Kernel::makeTopicKey("live.wifi");
const Kernel::TopicKey kMqttStateTopic = Kernel::makeTopicKey("live.mqtt");
const Kernel::TopicKey kTelemetryTopic = Kernel::makeTopicKey("live.telemetry");

WiFiClient g_httpClient;
WiFiClient g_mqttTransport;
PubSubClient g_mqttClient(g_mqttTransport);
IPAddress g_httpAddress;
IPAddress g_mqttAddress;
bool g_httpAddressValid = false;
bool g_mqttAddressValid = false;

ZeroWiFiMaintainer g_wifiMaintainer;
ZeroHttpPump g_httpPump;
ZeroMqttPump g_mqttPump;

char g_httpPayload[96];
bool g_httpRequestPrepared = false;
bool g_httpResponseSawStatus = false;
bool g_httpResponseSuccess = false;
unsigned long g_httpReadDeadlineMs = 0;
char g_httpStatusLine[64];
uint8_t g_httpStatusLineLength = 0;

unsigned long g_startedAtMs = 0;
unsigned long g_lastSummaryAtMs = 0;
unsigned long g_lastHttpDispatchAtMs = 0;
unsigned long g_lastMqttDispatchAtMs = 0;
unsigned long g_nextExpectedUs = 0;
unsigned long g_sampleRuns = 0;
unsigned long g_lagAccumUs = 0;
unsigned long g_maxLagUs = 0;
unsigned long g_fastMisses = 0;
unsigned long g_sensorValue = 1200;

unsigned long boardMillis() {
  return millis();
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void connectWiFi() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    return;
  }
#if defined(ARDUINO_ARCH_ESP8266)
  if (status == WL_DISCONNECTED || status == WL_IDLE_STATUS ||
      status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED ||
      status == WL_CONNECTION_LOST) {
    WiFi.begin(kWiFiSsid, kWiFiPassword);
  }
#else
  if (status == WL_IDLE_STATUS) {
    return;
  }
  WiFi.begin(kWiFiSsid, kWiFiPassword);
#endif
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  g_mqttClient.disconnect();
}

void closeHttpClient() {
#if defined(ARDUINO_ARCH_ESP8266)
  g_httpClient.abort();
#else
  g_httpClient.stop();
#endif
}

ZeroHttpPump::StepResult httpConnectStep(const ZeroHttpPump::Request&, void*) {
  closeHttpClient();
  g_httpClient.setTimeout(kHttpIoTimeoutMs);
  const bool connected = g_httpAddressValid ? g_httpClient.connect(g_httpAddress, kHttpPort)
                                            : g_httpClient.connect(kHttpHost, kHttpPort);
  if (!connected) {
    closeHttpClient();
    return ZeroHttpPump::kStepFailed;
  }
  g_httpRequestPrepared = false;
  g_httpResponseSawStatus = false;
  g_httpResponseSuccess = false;
  g_httpReadDeadlineMs = 0;
  g_httpStatusLineLength = 0;
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpWriteStep(const ZeroHttpPump::Request& request, void*) {
  if (!g_httpClient.connected()) {
    return ZeroHttpPump::kStepFailed;
  }

  if (!g_httpRequestPrepared) {
    g_httpClient.print("POST ");
    g_httpClient.print(kHttpPath);
    g_httpClient.print(" HTTP/1.1\r\nHost: ");
    g_httpClient.print(kHttpHost);
    g_httpClient.print("\r\nConnection: close\r\nContent-Type: ");
    g_httpClient.print(request.contentType == NULL ? "application/json" : request.contentType);
    g_httpClient.print("\r\nContent-Length: ");
    g_httpClient.print(request.bodyLength);
    g_httpClient.print("\r\n\r\n");
    g_httpClient.write(reinterpret_cast<const uint8_t*>(request.body), request.bodyLength);
    g_httpRequestPrepared = true;
    g_httpReadDeadlineMs = millis() + kHttpIoTimeoutMs;
  }

  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult httpReadStep(const ZeroHttpPump::Request&, void*) {
  if (!g_httpClient.connected() && !g_httpClient.available()) {
    if (g_httpResponseSawStatus && g_httpResponseSuccess) {
      return ZeroHttpPump::kStepComplete;
    }
    closeHttpClient();
    return ZeroHttpPump::kStepFailed;
  }

  while (g_httpClient.available()) {
    const char ch = static_cast<char>(g_httpClient.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      if (g_httpStatusLineLength > 0) {
        g_httpStatusLine[g_httpStatusLineLength] = '\0';
        if (!g_httpResponseSawStatus) {
          g_httpResponseSawStatus = true;
          g_httpResponseSuccess =
              (strncmp(g_httpStatusLine, "HTTP/1.1 2", 10) == 0) ||
              (strncmp(g_httpStatusLine, "HTTP/1.0 2", 10) == 0);
          g_httpStatusLineLength = 0;
          if (g_httpResponseSuccess) {
            return ZeroHttpPump::kStepComplete;
          }
          closeHttpClient();
          return ZeroHttpPump::kStepFailed;
        }
      }
      g_httpStatusLineLength = 0;
      continue;
    }
    if (g_httpStatusLineLength < (sizeof(g_httpStatusLine) - 1)) {
      g_httpStatusLine[g_httpStatusLineLength++] = ch;
    }
  }

  if (millis() > g_httpReadDeadlineMs) {
    closeHttpClient();
    return ZeroHttpPump::kStepFailed;
  }

  return ZeroHttpPump::kStepPending;
}

ZeroHttpPump::StepResult httpCloseStep(const ZeroHttpPump::Request&, void*) {
  closeHttpClient();
  g_httpRequestPrepared = false;
  g_httpResponseSawStatus = false;
  g_httpResponseSuccess = false;
  g_httpStatusLineLength = 0;
  return ZeroHttpPump::kStepComplete;
}

bool mqttLinkProbe() {
  return g_mqttClient.connected();
}

bool mqttConnectStep(void*) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  char clientId[48];
  snprintf(clientId, sizeof(clientId), "zk-live-%lu", millis());
  return g_mqttClient.connect(clientId);
}

void mqttLoopStep(void*) {
  g_mqttClient.loop();
}

bool mqttPublishStep(Kernel::TopicKey, const Kernel::EventValue& value, void*) {
  char payload[32];
  unsigned long numeric = 0;
  if (value.type == Kernel::kEventUnsigned) {
    numeric = value.unsignedValue;
  } else if (value.type == Kernel::kEventLong) {
    numeric = static_cast<unsigned long>(value.longValue);
  } else if (value.type == Kernel::kEventBool) {
    numeric = value.boolValue ? 1UL : 0UL;
  }
  snprintf(payload, sizeof(payload), "%lu", numeric);
  return g_mqttClient.publish(kMqttTopic, payload);
}

void sampleTask() {
  const unsigned long nowUs = micros();
  if (g_nextExpectedUs == 0) {
    g_nextExpectedUs = nowUs;
  }

#if defined(ARDUINO_ARCH_ESP8266)
  const unsigned long lagUs = nowUs > g_nextExpectedUs ? nowUs - g_nextExpectedUs : 0;
#else
  const long deltaUs = static_cast<long>(nowUs - g_nextExpectedUs);
  if (deltaUs < 0) {
    return;
  }

  const unsigned long lagUs = static_cast<unsigned long>(deltaUs);
#endif
  g_lagAccumUs += lagUs;
  if (lagUs > g_maxLagUs) {
    g_maxLagUs = lagUs;
  }
  if (lagUs > kMissThresholdUs) {
    ++g_fastMisses;
  }

  ++g_sampleRuns;
  ++g_sensorValue;
  g_nextExpectedUs += kSamplePeriodUs;
  if (nowUs > g_nextExpectedUs + kSamplePeriodUs) {
    g_nextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void dispatchTask() {
  const unsigned long nowMs = millis();

  const bool httpDue = kHttpDispatchPeriodMs > 0 &&
                       (nowMs - g_lastHttpDispatchAtMs) >= kHttpDispatchPeriodMs;
  const bool mqttDue = (nowMs - g_lastMqttDispatchAtMs) >= kMqttDispatchPeriodMs;

  if (httpDue && g_httpPump.queuedCount() == 0 && !g_httpPump.isBusy()) {
    g_lastHttpDispatchAtMs = nowMs;
    const int written = snprintf(g_httpPayload, sizeof(g_httpPayload),
                                 "{\"seq\":%lu,\"sensor\":%lu,\"board\":\"module\"}",
                                 g_sampleRuns, g_sensorValue);
    if (written > 0) {
      ZeroHttpPump::Request request;
      request.path = kHttpPath;
      request.contentType = "application/json";
      request.body = g_httpPayload;
      request.bodyLength = static_cast<uint16_t>(written);
      g_httpPump.enqueue(request);
    }
  }

  if (mqttDue) {
    g_lastMqttDispatchAtMs = nowMs;
    if (g_mqttPump.queuedCount() < ZeroMqttPump::kQueueCapacity) {
      g_mqttPump.enqueue(kTelemetryTopic, Kernel::EventValue::fromUnsigned(g_sensorValue), 1);
    }
  }
}

void wifiMaintainerTask() {
  g_wifiMaintainer.tick();
}

void httpPumpTask() {
  g_httpPump.tick();
}

void mqttPumpTask() {
  g_mqttPump.tick();
}

unsigned long percentage(unsigned long ok, unsigned long fail) {
  const unsigned long total = ok + fail;
  if (total == 0) {
    return 0;
  }
  return (ok * 100UL) / total;
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - g_lastSummaryAtMs) < kSummaryPeriodMs) {
    return;
  }
  g_lastSummaryAtMs = nowMs;

  const unsigned long avgLagUs = g_sampleRuns == 0 ? 0 : g_lagAccumUs / g_sampleRuns;
  const ZeroTransportMetrics::Snapshot httpMetrics = g_httpPump.metrics().snapshot();
  const ZeroTransportMetrics::Snapshot mqttMetrics = g_mqttPump.metrics().snapshot();
  char line[352];
  snprintf(line, sizeof(line),
           "LIVE_NETMODULES window_ms=%lu sample_runs=%lu fast_avg_lag_us=%lu "
           "fast_max_lag_us=%lu fast_miss=%lu wifi_attempts=%lu wifi_reconnects=%lu "
           "http_connect_ok=%lu http_connect_fail=%lu http_ok=%lu http_fail=%lu "
           "http_rate=%lu http_phase_to=%lu mqtt_connect_ok=%lu mqtt_connect_fail=%lu "
           "mqtt_ok=%lu mqtt_fail=%lu mqtt_rate=%lu http_queue=%u mqtt_queue=%u",
           nowMs - g_startedAtMs,
           g_sampleRuns,
           avgLagUs,
           g_maxLagUs,
           g_fastMisses,
           g_wifiMaintainer.connectAttempts(),
           g_wifiMaintainer.reconnectTransitions(),
           httpMetrics.connectSuccesses,
           httpMetrics.connectFailures,
           httpMetrics.sendSuccesses,
           httpMetrics.sendFailures,
           percentage(httpMetrics.sendSuccesses, httpMetrics.sendFailures),
           httpMetrics.phaseTimeouts,
           mqttMetrics.connectSuccesses,
           mqttMetrics.connectFailures,
           mqttMetrics.sendSuccesses,
           mqttMetrics.sendFailures,
           percentage(mqttMetrics.sendSuccesses, mqttMetrics.sendFailures),
           g_httpPump.queuedCount(),
           g_mqttPump.queuedCount());
  Serial.println(line);
}

void onTypedState(const char*, const Kernel::EventValue&) {}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
#if defined(ARDUINO_ARCH_ESP32)
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
#elif defined(ARDUINO_ARCH_ESP8266)
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);
#endif

  g_httpAddressValid = g_httpAddress.fromString(kHttpHost);
  g_mqttAddressValid = g_mqttAddress.fromString(kMqttHost);
  if (g_mqttAddressValid) {
    g_mqttClient.setServer(g_mqttAddress, kMqttPort);
  } else {
    g_mqttClient.setServer(kMqttHost, kMqttPort);
  }
  g_mqttClient.setSocketTimeout(1);
  g_mqttClient.setKeepAlive(15);
  g_httpClient.setNoDelay(true);
  g_mqttTransport.setNoDelay(true);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(Kernel::kIdleSleep);
  ZeroKernel.subscribeTypedFast(kWiFiStateTopic, onTypedState);
  ZeroKernel.subscribeTypedFast(kMqttStateTopic, onTypedState);

  ZeroWiFiMaintainer::Config wifiConfig;
#if defined(ARDUINO_ARCH_ESP8266)
  wifiConfig = ZeroNetProfileEsp8266::wifiConfig();
#endif
  wifiConfig.manageCapabilities = true;
  wifiConfig.capabilityMask = Kernel::kCapNetwork;
  wifiConfig.stateTopicKey = kWiFiStateTopic;
  g_wifiMaintainer.begin(ZeroKernel, isWiFiConnected, connectWiFi, disconnectWiFi, wifiConfig);

  ZeroHttpPump::Config httpConfig;
#if defined(ARDUINO_ARCH_ESP8266)
  httpConfig = ZeroNetProfileEsp8266::httpConfig();
#endif
  g_httpPump.begin(ZeroKernel,
                   httpConnectStep,
                   httpWriteStep,
                   httpReadStep,
                   httpCloseStep,
                   httpConfig);
  g_httpPump.setLinkProbe(isWiFiConnected);

  ZeroMqttPump::Config mqttConfig;
#if defined(ARDUINO_ARCH_ESP8266)
  mqttConfig = ZeroNetProfileEsp8266::mqttConfig();
#endif
  mqttConfig.stateTopicKey = kMqttStateTopic;
  g_mqttPump.begin(ZeroKernel,
                   mqttLinkProbe,
                   mqttConnectStep,
                   mqttLoopStep,
                   mqttPublishStep,
                   mqttConfig);
  g_mqttPump.setTransportProbe(isWiFiConnected);

#if defined(ARDUINO_ARCH_ESP8266)
  ZeroKernel.addTask("Sample", sampleTask, ZeroNetProfileEsp8266::kSampleTaskIntervalMs, 0);
  ZeroKernel.addTask("WiFiMaint", wifiMaintainerTask, ZeroNetProfileEsp8266::kWiFiTaskIntervalMs,
                     ZeroNetProfileEsp8266::kWiFiTaskStartDelayMs);
  ZeroKernel.addTask("HttpPump", httpPumpTask, ZeroNetProfileEsp8266::kHttpTaskIntervalMs,
                     ZeroNetProfileEsp8266::kHttpTaskStartDelayMs);
  ZeroKernel.addTask("MqttPump", mqttPumpTask, ZeroNetProfileEsp8266::kMqttTaskIntervalMs,
                     ZeroNetProfileEsp8266::kMqttTaskStartDelayMs);
  ZeroKernel.addTask("Dispatch", dispatchTask, ZeroNetProfileEsp8266::kDispatchTaskIntervalMs,
                     ZeroNetProfileEsp8266::kDispatchTaskStartDelayMs);
#else
  ZeroKernel.addTask("Sample", sampleTask, kSampleTaskIntervalMs, 0);
  ZeroKernel.addTask("WiFiMaint", wifiMaintainerTask, 100, kWiFiMaintStartDelayMs);
  ZeroKernel.addTask("HttpPump", httpPumpTask, 100, kHttpPumpStartDelayMs);
  ZeroKernel.addTask("MqttPump", mqttPumpTask, 100, kMqttPumpStartDelayMs);
  ZeroKernel.addTask("Dispatch", dispatchTask, 100, kDispatchStartDelayMs);
#endif
  ZeroKernel.addTask("Report", reportTask, 250, kReportStartDelayMs);

  ZeroKernel.setTaskPriority("Sample", Kernel::kPriorityCritical);
  ZeroKernel.setTaskPriority("WiFiMaint", Kernel::kPriorityHigh);
  ZeroKernel.setTaskPriority("HttpPump", Kernel::kPriorityNormal);
  ZeroKernel.setTaskPriority("MqttPump", Kernel::kPriorityNormal);
  ZeroKernel.setTaskPriority("Dispatch", Kernel::kPriorityNormal);
  ZeroKernel.setTaskPriority("Report", Kernel::kPriorityLow);

  g_startedAtMs = millis();
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
