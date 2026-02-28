#include <WiFi.h>

namespace {

const char* kSsid = "replace-with-ssid";
const char* kPassword = "replace-with-password";

unsigned long gLastWiFiAtMs = 0;
unsigned long gLastSampleAtMs = 0;
unsigned long gLastTelemetryAtMs = 0;
unsigned long gLastHeartbeatAtMs = 0;
unsigned long gLastModeAtMs = 0;
unsigned long gLastStatsAtMs = 0;
unsigned long gSummaryStartedAtMs = 0;

unsigned long gSampleSequence = 0;
unsigned long gLastSampleValue = 0;
unsigned long gHeartbeatCount = 0;
unsigned long gLastConnectAttemptMs = 0;
bool gDiagnosticsEnabled = true;

uint32_t gLoopCount = 0;
uint32_t gSampleRuns = 0;
uint32_t gTelemetryRuns = 0;
uint32_t gWiFiRuns = 0;
uint32_t gStatsRuns = 0;
uint32_t gDiagToggles = 0;
uint32_t gFastMiss = 0;
uint32_t gFastMaxLagUs = 0;
uint64_t gFastLagTotalUs = 0;
uint32_t gSummaryWindows = 0;
unsigned long gExpectedSampleAtUs = 0;

bool credentialsLookConfigured() {
  return kSsid[0] != '\0' && kPassword[0] != '\0' &&
         strcmp(kSsid, "replace-with-ssid") != 0 &&
         strcmp(kPassword, "replace-with-password") != 0;
}

void resetWindow(unsigned long nowMs) {
  gLoopCount = 0;
  gSampleRuns = 0;
  gTelemetryRuns = 0;
  gWiFiRuns = 0;
  gStatsRuns = 0;
  gDiagToggles = 0;
  gFastMiss = 0;
  gFastMaxLagUs = 0;
  gFastLagTotalUs = 0;
  gSummaryStartedAtMs = nowMs;
}

void printSummary(unsigned long nowMs) {
  const uint32_t averageLagUs =
      gSampleRuns == 0 ? 0 : static_cast<uint32_t>(gFastLagTotalUs / gSampleRuns);

  Serial.print("BASELINE_TELEMETRY window_ms=");
  Serial.print(nowMs - gSummaryStartedAtMs);
  Serial.print(" sample_runs=");
  Serial.print(gSampleRuns);
  Serial.print(" telemetry_runs=");
  Serial.print(gTelemetryRuns);
  Serial.print(" wifi_runs=");
  Serial.print(gWiFiRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(averageLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(gFastMaxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(gFastMiss);
  Serial.print(" diag_toggles=");
  Serial.print(gDiagToggles);
  Serial.print(" loop_count=");
  Serial.println(gLoopCount);

  ++gSummaryWindows;
  resetWindow(nowMs);
}

void runWiFiMaintainer(unsigned long nowMs) {
  ++gWiFiRuns;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WIFI status=connected ssid=");
    Serial.println(credentialsLookConfigured() ? kSsid : "not-configured");
    return;
  }

  if (credentialsLookConfigured() && (nowMs - gLastConnectAttemptMs) >= 4000UL) {
    gLastConnectAttemptMs = nowMs;
    WiFi.disconnect();
    WiFi.begin(kSsid, kPassword);
  }

  Serial.print("WIFI status=reconnecting ssid=");
  Serial.println(credentialsLookConfigured() ? kSsid : "not-configured");
  delay(8);
}

void runSensorSampler(unsigned long nowMs) {
  const unsigned long nowUs = micros();
  if (gExpectedSampleAtUs == 0UL) {
    gExpectedSampleAtUs = nowUs;
  } else {
    const unsigned long lagUs =
        nowUs > gExpectedSampleAtUs ? (nowUs - gExpectedSampleAtUs) : 0UL;
    gFastLagTotalUs += lagUs;
    if (lagUs > gFastMaxLagUs) {
      gFastMaxLagUs = lagUs;
    }
    if (lagUs > 2000UL) {
      ++gFastMiss;
    }
  }

  gExpectedSampleAtUs += 250000UL;
  ++gSampleRuns;
  ++gSampleSequence;
  gLastSampleValue = 200UL + ((nowMs / 25UL) % 75UL);
}

void runTelemetryFlush() {
  ++gTelemetryRuns;
  Serial.print("SAMPLE seq=");
  Serial.print(gSampleSequence);
  Serial.print(" value=");
  Serial.println(gLastSampleValue);
}

void runHeartbeat() {
  ++gHeartbeatCount;
  Serial.print("HEARTBEAT tick=");
  Serial.println(gHeartbeatCount);
}

void runDiagModeToggle() {
  gDiagnosticsEnabled = !gDiagnosticsEnabled;
  ++gDiagToggles;
  Serial.print("DIAG mode=");
  Serial.println(gDiagnosticsEnabled ? "enabled" : "paused");
}

void runStatsPrinter() {
  ++gStatsRuns;
  Serial.print("STATS samples=");
  Serial.print(gSampleRuns);
  Serial.print(" telemetry=");
  Serial.print(gTelemetryRuns);
  Serial.print(" wifi=");
  Serial.print(gWiFiRuns);
  Serial.print(" diag=");
  Serial.println(gDiagnosticsEnabled ? "on" : "off");
  delay(10);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  WiFi.mode(WIFI_STA);
  resetWindow(millis());
}

void loop() {
  const unsigned long nowMs = millis();
  ++gLoopCount;

  if ((nowMs - gLastWiFiAtMs) >= 2000UL) {
    gLastWiFiAtMs = nowMs;
    runWiFiMaintainer(nowMs);
  }

  if ((nowMs - gLastSampleAtMs) >= 250UL) {
    gLastSampleAtMs += 250UL;
    if (gLastSampleAtMs == 0UL) {
      gLastSampleAtMs = nowMs;
    }
    runSensorSampler(nowMs);
  }

  if ((nowMs - gLastTelemetryAtMs) >= 1000UL) {
    gLastTelemetryAtMs = nowMs;
    runTelemetryFlush();
  }

  if ((nowMs - gLastHeartbeatAtMs) >= 3000UL) {
    gLastHeartbeatAtMs = nowMs;
    runHeartbeat();
  }

  if ((nowMs - gLastModeAtMs) >= 6000UL) {
    gLastModeAtMs = nowMs;
    runDiagModeToggle();
  }

  if (gDiagnosticsEnabled && (nowMs - gLastStatsAtMs) >= 5000UL) {
    gLastStatsAtMs = nowMs;
    runStatsPrinter();
  }

  if ((nowMs - gSummaryStartedAtMs) >= 5000UL) {
    printSummary(nowMs);
  }
}
