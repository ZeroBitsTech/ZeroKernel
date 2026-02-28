#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>

using zerokernel::Kernel;

namespace {

const unsigned long kSamplePeriodUs = 100000UL;
const unsigned long kSummaryPeriodMs = 5000UL;

struct BaselineState {
  bool wifiConnected;
  bool connectPending;
  bool mqttConnected;
  bool httpConnectPending;
  bool failNextHttp;
  bool failNextMqtt;
  unsigned long nextExpectedUs;
  unsigned long lagAccumUs;
  unsigned long maxLagUs;
  unsigned long sampleRuns;
  unsigned long fastMisses;
  unsigned long startedAtMs;
  unsigned long lastSummaryAtMs;
  unsigned long httpQueued;
  unsigned long httpSendOk;
  unsigned long httpSendFail;
  unsigned long httpConnectAttempts;
  unsigned long httpMaxQueue;
  unsigned long mqttQueued;
  unsigned long mqttSendOk;
  unsigned long mqttSendFail;
  unsigned long mqttConnectAttempts;
  unsigned long wifiConnectAttempts;
  unsigned long wifiReconnects;
} g_state = {};

unsigned long boardMillis() {
  return millis();
}

void sampleTask() {
  const unsigned long nowUs = micros();
  if (g_state.nextExpectedUs == 0) {
    g_state.nextExpectedUs = nowUs;
  }

  const unsigned long lagUs = nowUs > g_state.nextExpectedUs ? nowUs - g_state.nextExpectedUs : 0;
  g_state.lagAccumUs += lagUs;
  if (lagUs > g_state.maxLagUs) {
    g_state.maxLagUs = lagUs;
  }
  if (lagUs > 1500UL) {
    ++g_state.fastMisses;
  }
  ++g_state.sampleRuns;
  g_state.nextExpectedUs += kSamplePeriodUs;
  if (nowUs > g_state.nextExpectedUs + kSamplePeriodUs) {
    g_state.nextExpectedUs = nowUs + kSamplePeriodUs;
  }
}

void queueTask() {
  if (g_state.httpQueued < 4UL) {
    ++g_state.httpQueued;
    if (g_state.httpQueued > g_state.httpMaxQueue) {
      g_state.httpMaxQueue = g_state.httpQueued;
    }
  }

  if (g_state.mqttQueued < 6UL) {
    ++g_state.mqttQueued;
  }
}

void wifiTask() {
  if (g_state.wifiConnected) {
    return;
  }

  ++g_state.wifiConnectAttempts;
  if (g_state.connectPending) {
    g_state.connectPending = false;
    return;
  }

  g_state.connectPending = true;
  g_state.wifiConnected = true;
  ++g_state.wifiReconnects;
  g_state.mqttConnected = true;
}

void httpTask() {
  if (g_state.httpQueued == 0) {
    return;
  }

  ++g_state.httpConnectAttempts;
  if (g_state.httpConnectPending) {
    g_state.httpConnectPending = false;
    return;
  }

  g_state.httpConnectPending = true;
  --g_state.httpQueued;
  if (g_state.failNextHttp) {
    g_state.failNextHttp = false;
    ++g_state.httpSendFail;
    return;
  }

  g_state.failNextHttp = true;
  ++g_state.httpSendOk;
}

void mqttTask() {
  if (!g_state.mqttConnected) {
    ++g_state.mqttConnectAttempts;
    g_state.mqttConnected = true;
    return;
  }

  if (g_state.mqttQueued == 0) {
    return;
  }

  --g_state.mqttQueued;
  if (g_state.failNextMqtt) {
    g_state.failNextMqtt = false;
    ++g_state.mqttSendFail;
    return;
  }

  g_state.failNextMqtt = true;
  ++g_state.mqttSendOk;
}

void reportTask() {
  const unsigned long nowMs = millis();
  if ((nowMs - g_state.lastSummaryAtMs) < kSummaryPeriodMs) {
    return;
  }

  g_state.lastSummaryAtMs = nowMs;
  const unsigned long windowMs = nowMs - g_state.startedAtMs;
  const unsigned long avgLagUs = g_state.sampleRuns == 0 ? 0 : g_state.lagAccumUs / g_state.sampleRuns;

  Serial.print("BASELINE_NETMODULES window_ms=");
  Serial.print(windowMs);
  Serial.print(" sample_runs=");
  Serial.print(g_state.sampleRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(avgLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(g_state.maxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(g_state.fastMisses);
  Serial.print(" wifi_attempts=");
  Serial.print(g_state.wifiConnectAttempts);
  Serial.print(" wifi_reconnects=");
  Serial.print(g_state.wifiReconnects);
  Serial.print(" http_ok=");
  Serial.print(g_state.httpSendOk);
  Serial.print(" http_fail=");
  Serial.print(g_state.httpSendFail);
  Serial.print(" mqtt_ok=");
  Serial.print(g_state.mqttSendOk);
  Serial.print(" mqtt_fail=");
  Serial.print(g_state.mqttSendFail);
  Serial.print(" queue_max=");
  Serial.println(g_state.httpMaxQueue);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  const unsigned long startedAtMs = millis();
  g_state.startedAtMs = startedAtMs;
  g_state.lastSummaryAtMs = startedAtMs;

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleSleep);
  ZeroKernel.addTask("Sample", sampleTask, 100, 0);
  ZeroKernel.addTask("Queue", queueTask, 250, 0);
  ZeroKernel.addTask("WiFi", wifiTask, 300, 0);
  ZeroKernel.addTask("Http", httpTask, 200, 0);
  ZeroKernel.addTask("Mqtt", mqttTask, 150, 0);
  ZeroKernel.addTask("Report", reportTask, 1000, 0);
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
}
