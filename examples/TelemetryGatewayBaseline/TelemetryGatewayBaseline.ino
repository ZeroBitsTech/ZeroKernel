#include <Arduino.h>

namespace {

const unsigned long kQueuePeriodMs = 180UL;
const unsigned long kLinkPeriodMs = 250UL;
const unsigned long kFlushPeriodMs = 100UL;
const unsigned long kReportPeriodMs = 5000UL;
const unsigned long kLinkDropPeriodMs = 12000UL;
const unsigned long kLinkRestoreDelayMs = 1500UL;

bool gLinkUp = false;
bool gReadyToConnect = false;
unsigned long gLastDropWindow = 0;
unsigned long gDropStartedAtMs = 0;
unsigned long gWifiAttempts = 0;
unsigned long gReconnects = 0;
unsigned long gHttpQueued = 0;
unsigned long gMqttQueued = 0;
unsigned long gHttpQueueMax = 0;
unsigned long gMqttQueueMax = 0;
unsigned long gHttpOk = 0;
unsigned long gHttpFail = 0;
unsigned long gMqttOk = 0;
unsigned long gMqttFail = 0;
unsigned long gHttpSendCalls = 0;
unsigned long gMqttSendCalls = 0;
unsigned long gLastQueueAtMs = 0;
unsigned long gLastLinkAtMs = 0;
unsigned long gLastFlushAtMs = 0;
unsigned long gWindowStartedAtMs = 0;

unsigned long percentage(unsigned long ok, unsigned long fail) {
  const unsigned long total = ok + fail;
  if (total == 0) {
    return 100;
  }
  return (ok * 100UL) / total;
}

void resetWindow(unsigned long nowMs) {
  gWindowStartedAtMs = nowMs;
  gHttpQueueMax = 0;
  gMqttQueueMax = 0;
  gHttpOk = 0;
  gHttpFail = 0;
  gMqttOk = 0;
  gMqttFail = 0;
}

void updateLinkState(unsigned long nowMs) {
  const unsigned long dropWindow = nowMs / kLinkDropPeriodMs;
  if (dropWindow > 0 && dropWindow != gLastDropWindow) {
    gLastDropWindow = dropWindow;
    gLinkUp = false;
    gReadyToConnect = false;
    gDropStartedAtMs = nowMs;
  }

  if (!gLinkUp) {
    ++gWifiAttempts;
    if (!gReadyToConnect) {
      gReadyToConnect = true;
      return;
    }

    if ((nowMs - gDropStartedAtMs) < kLinkRestoreDelayMs) {
      return;
    }

    gLinkUp = true;
    ++gReconnects;
    gDropStartedAtMs = 0;
  }
}

void queueTelemetry() {
  if (gHttpQueued < 4UL) {
    ++gHttpQueued;
  }
  if (gMqttQueued < 4UL) {
    ++gMqttQueued;
  }

  if (gHttpQueued > gHttpQueueMax) {
    gHttpQueueMax = gHttpQueued;
  }
  if (gMqttQueued > gMqttQueueMax) {
    gMqttQueueMax = gMqttQueued;
  }
}

void flushTelemetry() {
  if (!gLinkUp) {
    return;
  }

  if (gHttpQueued > 0) {
    ++gHttpSendCalls;
    if ((gHttpSendCalls % 7UL) == 0) {
      ++gHttpFail;
    } else {
      ++gHttpOk;
      --gHttpQueued;
    }
  }

  if (gMqttQueued > 0) {
    ++gMqttSendCalls;
    if ((gMqttSendCalls % 9UL) == 0) {
      ++gMqttFail;
    } else {
      ++gMqttOk;
      --gMqttQueued;
    }
  }
}

void maybeReport(unsigned long nowMs) {
  if ((nowMs - gWindowStartedAtMs) < kReportPeriodMs) {
    return;
  }

  Serial.print("BASELINE_TELEMETRY_GATEWAY window_ms=");
  Serial.print(nowMs - gWindowStartedAtMs);
  Serial.print(" link_up=");
  Serial.print(gLinkUp ? 1 : 0);
  Serial.print(" wifi_attempts=");
  Serial.print(gWifiAttempts);
  Serial.print(" reconnects=");
  Serial.print(gReconnects);
  Serial.print(" http_ok=");
  Serial.print(gHttpOk);
  Serial.print(" http_fail=");
  Serial.print(gHttpFail);
  Serial.print(" http_rate=");
  Serial.print(percentage(gHttpOk, gHttpFail));
  Serial.print(" mqtt_ok=");
  Serial.print(gMqttOk);
  Serial.print(" mqtt_fail=");
  Serial.print(gMqttFail);
  Serial.print(" mqtt_rate=");
  Serial.print(percentage(gMqttOk, gMqttFail));
  Serial.print(" http_queue=");
  Serial.print(gHttpQueued);
  Serial.print(" mqtt_queue=");
  Serial.println(gMqttQueued);

  resetWindow(nowMs);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  const unsigned long nowMs = millis();
  gLastQueueAtMs = nowMs;
  gLastLinkAtMs = nowMs;
  gLastFlushAtMs = nowMs;
  resetWindow(nowMs);
}

void loop() {
  const unsigned long nowMs = millis();

  if ((nowMs - gLastLinkAtMs) >= kLinkPeriodMs) {
    gLastLinkAtMs += kLinkPeriodMs;
    updateLinkState(nowMs);
  }

  if ((nowMs - gLastQueueAtMs) >= kQueuePeriodMs) {
    gLastQueueAtMs += kQueuePeriodMs;
    queueTelemetry();
  }

  if ((nowMs - gLastFlushAtMs) >= kFlushPeriodMs) {
    gLastFlushAtMs += kFlushPeriodMs;
    flushTelemetry();
  }

  maybeReport(nowMs);
}
