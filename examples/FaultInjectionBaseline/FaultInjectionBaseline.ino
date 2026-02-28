namespace {

enum DemoState : uint8_t {
  kNormal = 0,
  kSafeMode = 1
};

unsigned long gLastHeartbeatAtMs = 0;
unsigned long gLastFaultAtMs = 0;
unsigned long gLastRecoveryAtMs = 0;
unsigned long gLastReportAtMs = 0;
unsigned long gSummaryStartedAtMs = 0;
unsigned long gSafeModeEnteredAtMs = 0;

unsigned long gHeartbeatCount = 0;
uint8_t gFaultBurstRemaining = 3;
bool gManualSafeModeTriggered = false;
uint8_t gState = kNormal;

uint32_t gHeartbeatRuns = 0;
uint32_t gFaultRuns = 0;
uint32_t gReportRuns = 0;
uint32_t gFailures = 0;
uint32_t gRecoveries = 0;
uint32_t gOverruns = 0;
uint32_t gFastMiss = 0;
uint32_t gFastMaxLagUs = 0;
uint64_t gFastLagTotalUs = 0;
unsigned long gExpectedHeartbeatAtUs = 0;

const char* stateName(uint8_t state) {
  return state == kSafeMode ? "SAFE_MODE" : "NORMAL";
}

void resetWindow(unsigned long nowMs) {
  gHeartbeatRuns = 0;
  gFaultRuns = 0;
  gReportRuns = 0;
  gFastMiss = 0;
  gFastMaxLagUs = 0;
  gFastLagTotalUs = 0;
  gSummaryStartedAtMs = nowMs;
}

void emitHeartbeat() {
  const unsigned long nowUs = micros();
  if (gExpectedHeartbeatAtUs == 0UL) {
    gExpectedHeartbeatAtUs = nowUs;
  } else {
    const unsigned long lagUs =
        nowUs > gExpectedHeartbeatAtUs ? (nowUs - gExpectedHeartbeatAtUs) : 0UL;
    gFastLagTotalUs += lagUs;
    if (lagUs > gFastMaxLagUs) {
      gFastMaxLagUs = lagUs;
    }
    if (lagUs > 2000UL) {
      ++gFastMiss;
    }
  }

  gExpectedHeartbeatAtUs += 500000UL;
  ++gHeartbeatRuns;
  ++gHeartbeatCount;
  Serial.print("HEALTH beat=");
  Serial.println(gHeartbeatCount);
}

void runFaultyWorker() {
  ++gFaultRuns;
  if (gFaultBurstRemaining > 0) {
    Serial.print("FAULT burst_remaining=");
    Serial.println(gFaultBurstRemaining);
    --gFaultBurstRemaining;
    ++gFailures;
    ++gOverruns;
    delay(12);
    return;
  }

  Serial.println("FAULT burst_remaining=0");
}

void runRecoverySupervisor(unsigned long nowMs) {
  if (gState != kSafeMode) {
    if (!gManualSafeModeTriggered && gFailures >= 3) {
      gManualSafeModeTriggered = true;
      gState = kSafeMode;
      gSafeModeEnteredAtMs = nowMs;
      Serial.println("STATE SAFE_MODE");
    }
    return;
  }

  if ((nowMs - gSafeModeEnteredAtMs) < 2500UL) {
    return;
  }

  Serial.println("RECOVERY resuming normal mode");
  gFaultBurstRemaining = 0;
  gState = kNormal;
  ++gRecoveries;
  Serial.println("STATE NORMAL");
}

void runReport() {
  ++gReportRuns;
  Serial.print("REPORT state=");
  Serial.print(stateName(gState));
  Serial.print(" failures=");
  Serial.print(gFailures);
  Serial.print(" recoveries=");
  Serial.print(gRecoveries);
  Serial.print(" overruns=");
  Serial.print(gOverruns);
  Serial.print(" runs=");
  Serial.println(gHeartbeatRuns + gFaultRuns + gReportRuns);
}

void printSummary(unsigned long nowMs) {
  const uint32_t averageLagUs =
      gHeartbeatRuns == 0 ? 0 : static_cast<uint32_t>(gFastLagTotalUs / gHeartbeatRuns);

  Serial.print("BASELINE_FAULT window_ms=");
  Serial.print(nowMs - gSummaryStartedAtMs);
  Serial.print(" heartbeat_runs=");
  Serial.print(gHeartbeatRuns);
  Serial.print(" fault_runs=");
  Serial.print(gFaultRuns);
  Serial.print(" fast_avg_lag_us=");
  Serial.print(averageLagUs);
  Serial.print(" fast_max_lag_us=");
  Serial.print(gFastMaxLagUs);
  Serial.print(" fast_miss=");
  Serial.print(gFastMiss);
  Serial.print(" failures=");
  Serial.print(gFailures);
  Serial.print(" recoveries=");
  Serial.print(gRecoveries);
  Serial.print(" overruns=");
  Serial.print(gOverruns);
  Serial.print(" state=");
  Serial.println(stateName(gState));

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  resetWindow(millis());
}

void loop() {
  const unsigned long nowMs = millis();

  if ((nowMs - gLastHeartbeatAtMs) >= 500UL) {
    gLastHeartbeatAtMs += 500UL;
    if (gLastHeartbeatAtMs == 0UL) {
      gLastHeartbeatAtMs = nowMs;
    }
    emitHeartbeat();
  }

  if (gState == kNormal && (nowMs - gLastFaultAtMs) >= 700UL) {
    gLastFaultAtMs += 700UL;
    if (gLastFaultAtMs == 0UL) {
      gLastFaultAtMs = nowMs;
    }
    runFaultyWorker();
  }

  if ((nowMs - gLastRecoveryAtMs) >= 500UL) {
    gLastRecoveryAtMs = nowMs;
    runRecoverySupervisor(nowMs);
  }

  if ((nowMs - gLastReportAtMs) >= 1500UL) {
    gLastReportAtMs = nowMs;
    runReport();
  }

  if ((nowMs - gSummaryStartedAtMs) >= 5000UL) {
    printSummary(nowMs);
  }
}
