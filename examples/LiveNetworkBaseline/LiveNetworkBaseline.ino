#include <ZeroKernel.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#error "LiveNetworkBaseline requires ESP8266 or ESP32."
#endif

#include <PubSubClient.h>
#include "LocalSecrets.h"

namespace {

WiFiClient g_httpClient;
WiFiClient g_mqttTransport;
PubSubClient g_mqttClient(g_mqttTransport);
IPAddress g_httpAddress;
IPAddress g_mqttAddress;
bool g_httpAddressValid = false;
bool g_mqttAddressValid = false;

const unsigned long kSamplePeriodUs = 100000UL;
const unsigned long kDispatchPeriodMs = 500UL;
const unsigned long kSummaryPeriodMs = 10000UL;
const unsigned long kMissThresholdUs = 1500UL;
const unsigned long kHttpTimeoutMs = 900UL;
const unsigned long kWiFiRetryBaseMs = 1000UL;
const unsigned long kWiFiRetryMaxMs = 16000UL;
const unsigned long kMqttRetryBaseMs = 1000UL;
const unsigned long kMqttRetryMaxMs = 12000UL;

unsigned long g_startedAtMs = 0;
unsigned long g_lastSummaryAtMs = 0;
unsigned long g_lastDispatchAtMs = 0;
unsigned long g_nextExpectedUs = 0;
unsigned long g_sampleRuns = 0;
unsigned long g_lagAccumUs = 0;
unsigned long g_maxLagUs = 0;
unsigned long g_fastMisses = 0;
unsigned long g_sensorValue = 900;
unsigned long g_wifiAttempts = 0;
unsigned long g_wifiTransitions = 0;
unsigned long g_httpOk = 0;
unsigned long g_httpFail = 0;
unsigned long g_mqttOk = 0;
unsigned long g_mqttFail = 0;
unsigned long g_queueMax = 0;
unsigned long g_nextWiFiAttemptAtMs = 0;
unsigned long g_currentWiFiRetryMs = kWiFiRetryBaseMs;
unsigned long g_nextMqttAttemptAtMs = 0;
unsigned long g_currentMqttRetryMs = kMqttRetryBaseMs;
bool g_lastWiFiConnected = false;

unsigned long applyBackoffWithJitter(unsigned long nowMs,
                                     unsigned long baseDelayMs,
                                     unsigned long* currentDelayMs,
                                     unsigned long maxDelayMs,
                                     unsigned long salt) {
  const unsigned long delayMs = (*currentDelayMs == 0) ? baseDelayMs : *currentDelayMs;
  const unsigned long jitterWindow = delayMs / 4UL;
  const unsigned long jitter = jitterWindow == 0 ? 0 : (salt % (jitterWindow + 1UL));
  unsigned long nextDelay = delayMs;
  if (maxDelayMs > 0 && nextDelay < maxDelayMs) {
    const unsigned long doubled = nextDelay * 2UL;
    nextDelay = doubled > maxDelayMs ? maxDelayMs : doubled;
  }
  *currentDelayMs = nextDelay;
  return nowMs + delayMs + jitter;
}

void ensureWiFi() {
  const unsigned long nowMs = millis();
  const bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected != g_lastWiFiConnected) {
    g_lastWiFiConnected = connected;
    if (connected) {
      ++g_wifiTransitions;
      g_currentWiFiRetryMs = kWiFiRetryBaseMs;
      g_nextWiFiAttemptAtMs = 0;
    } else {
      g_mqttClient.disconnect();
    }
  }

  if (connected) {
    return;
  }

  if (g_nextWiFiAttemptAtMs != 0 && nowMs < g_nextWiFiAttemptAtMs) {
    return;
  }

#if defined(ARDUINO_ARCH_ESP8266)
  if (WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_IDLE_STATUS ||
      WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED ||
      WiFi.status() == WL_CONNECTION_LOST) {
    WiFi.begin(kWiFiSsid, kWiFiPassword);
  }
#else
  WiFi.begin(kWiFiSsid, kWiFiPassword);
#endif
  ++g_wifiAttempts;
  g_nextWiFiAttemptAtMs =
      applyBackoffWithJitter(nowMs, kWiFiRetryBaseMs, &g_currentWiFiRetryMs,
                             kWiFiRetryMaxMs, g_wifiAttempts);
}

bool ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    g_mqttClient.disconnect();
    return false;
  }

  if (!g_mqttClient.connected()) {
    const unsigned long nowMs = millis();
    if (g_nextMqttAttemptAtMs != 0 && nowMs < g_nextMqttAttemptAtMs) {
      return false;
    }

    char clientId[48];
    snprintf(clientId, sizeof(clientId), "zk-baseline-%lu", nowMs);
    if (!g_mqttClient.connect(clientId)) {
      g_nextMqttAttemptAtMs =
          applyBackoffWithJitter(nowMs, kMqttRetryBaseMs, &g_currentMqttRetryMs,
                                 kMqttRetryMaxMs, g_httpFail + g_mqttFail + 1UL);
      return false;
    }

    g_currentMqttRetryMs = kMqttRetryBaseMs;
    g_nextMqttAttemptAtMs = 0;
  }

  g_mqttClient.loop();
  return g_mqttClient.connected();
}

bool sendHttpNow(const char* body, size_t length) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  g_httpClient.stop();
  g_httpClient.setTimeout(kHttpTimeoutMs);
  const bool connected = g_httpAddressValid ? g_httpClient.connect(g_httpAddress, kHttpPort)
                                            : g_httpClient.connect(kHttpHost, kHttpPort);
  if (!connected) {
    g_httpClient.stop();
    return false;
  }

  g_httpClient.print("POST ");
  g_httpClient.print(kHttpPath);
  g_httpClient.print(" HTTP/1.1\r\nHost: ");
  g_httpClient.print(kHttpHost);
  g_httpClient.print("\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
  g_httpClient.print(length);
  g_httpClient.print("\r\n\r\n");
  g_httpClient.write(reinterpret_cast<const uint8_t*>(body), length);

  const unsigned long deadline = millis() + kHttpTimeoutMs;
  String statusLine;
  while (millis() < deadline) {
    while (g_httpClient.available()) {
      const char ch = static_cast<char>(g_httpClient.read());
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        if (statusLine.length() > 0) {
          const bool ok =
              statusLine.startsWith("HTTP/1.1 2") || statusLine.startsWith("HTTP/1.0 2");
          g_httpClient.stop();
          return ok;
        }
        continue;
      }
      if (statusLine.length() < 64) {
        statusLine += ch;
      }
    }
#if defined(ARDUINO_ARCH_ESP8266)
    yield();
#else
    delay(1);
#endif
  }

  g_httpClient.stop();
  return false;
}

void sampleTask() {
  const unsigned long nowUs = micros();
  if (g_nextExpectedUs == 0) {
    g_nextExpectedUs = nowUs;
  }

  if (nowUs < g_nextExpectedUs) {
    return;
  }

  const unsigned long lagUs = nowUs - g_nextExpectedUs;
  g_lagAccumUs += lagUs;
  if (lagUs > g_maxLagUs) {
    g_maxLagUs = lagUs;
  }
  if (lagUs > kMissThresholdUs) {
    ++g_fastMisses;
  }

  ++g_sampleRuns;
  ++g_sensorValue;
  do {
    g_nextExpectedUs += kSamplePeriodUs;
  } while (g_nextExpectedUs <= nowUs);

  if (nowUs > g_nextExpectedUs + (kSamplePeriodUs * 2UL)) {
    g_nextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void dispatchTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - g_lastDispatchAtMs) < kDispatchPeriodMs) {
    return;
  }
  g_lastDispatchAtMs = nowMs;

  char payload[96];
  const int written = snprintf(payload, sizeof(payload),
                               "{\"seq\":%lu,\"sensor\":%lu,\"board\":\"baseline\"}",
                               g_sampleRuns, g_sensorValue);
  if (written <= 0) {
    return;
  }

  const bool httpOk = sendHttpNow(payload, static_cast<size_t>(written));
  if (httpOk) {
    ++g_httpOk;
  } else {
    ++g_httpFail;
  }

  const bool mqttConnected = ensureMqtt();
  if (mqttConnected && g_mqttClient.publish(kMqttTopic, payload)) {
    ++g_mqttOk;
  } else {
    ++g_mqttFail;
  }
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
  char line[320];
  snprintf(line, sizeof(line),
           "BASELINE_LIVE_NET window_ms=%lu sample_runs=%lu fast_avg_lag_us=%lu "
           "fast_max_lag_us=%lu fast_miss=%lu wifi_attempts=%lu wifi_reconnects=%lu "
           "http_ok=%lu http_fail=%lu http_rate=%lu mqtt_ok=%lu mqtt_fail=%lu "
           "mqtt_rate=%lu queue_max=%lu",
           nowMs - g_startedAtMs,
           g_sampleRuns,
           avgLagUs,
           g_maxLagUs,
           g_fastMisses,
           g_wifiAttempts,
           g_wifiTransitions,
           g_httpOk,
           g_httpFail,
           percentage(g_httpOk, g_httpFail),
           g_mqttOk,
           g_mqttFail,
           percentage(g_mqttOk, g_mqttFail),
           g_queueMax);
  Serial.println(line);
}

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
  g_startedAtMs = millis();
}

void loop() {
  ensureWiFi();
  ensureMqtt();
  sampleTask();
  dispatchTask();
  reportTask();
  delay(1);
}
