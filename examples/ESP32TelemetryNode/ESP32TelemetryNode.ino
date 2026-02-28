#include <WiFi.h>

#include <ZeroKernel.h>
#include <adapters/Esp32Clock.h>

namespace {

const char* kSsid = "replace-with-ssid";
const char* kPassword = "replace-with-password";

const zerokernel::Kernel::TopicKey kWifiStateKey =
    zerokernel::Kernel::makeTopicKey("telemetry.wifi");
const zerokernel::Kernel::TopicKey kSampleKey =
    zerokernel::Kernel::makeTopicKey("telemetry.sample");
const zerokernel::Kernel::TopicKey kHeartbeatKey =
    zerokernel::Kernel::makeTopicKey("telemetry.heartbeat");
const zerokernel::Kernel::TopicKey kDiagCommandKey =
    zerokernel::Kernel::makeTopicKey("telemetry.diag");

unsigned long gSampleSequence = 0;
unsigned long gLastSample = 0;
unsigned long gHeartbeatCount = 0;
unsigned long gLastConnectAttemptMs = 0;
bool gDiagnosticsEnabled = true;

const char* stateName(uint8_t state) {
  switch (state) {
    case zerokernel::Kernel::kStateBoot:
      return "BOOT";
    case zerokernel::Kernel::kStateNormal:
      return "NORMAL";
    case zerokernel::Kernel::kStateDegraded:
      return "DEGRADED";
    case zerokernel::Kernel::kStateSafeMode:
      return "SAFE_MODE";
    case zerokernel::Kernel::kStateRecovery:
      return "RECOVERY";
    case zerokernel::Kernel::kStatePanic:
      return "PANIC";
    default:
      return "UNKNOWN";
  }
}

const char* signalName(uint8_t signalType) {
  switch (signalType) {
    case zerokernel::Kernel::kSignalTaskFailure:
      return "TASK_FAILURE";
    case zerokernel::Kernel::kSignalHeartbeatTimeout:
      return "HEARTBEAT_TIMEOUT";
    case zerokernel::Kernel::kSignalDeadlineMiss:
      return "DEADLINE_MISS";
    case zerokernel::Kernel::kSignalEventDrop:
      return "EVENT_DROP";
    case zerokernel::Kernel::kSignalExecutionOverrun:
      return "OVERRUN";
    case zerokernel::Kernel::kSignalCommandDrop:
      return "COMMAND_DROP";
    case zerokernel::Kernel::kSignalWorkDrop:
      return "WORK_DROP";
    default:
      return "UNKNOWN";
  }
}

bool credentialsLookConfigured() {
  return kSsid[0] != '\0' && kPassword[0] != '\0' &&
         strcmp(kSsid, "replace-with-ssid") != 0 &&
         strcmp(kPassword, "replace-with-password") != 0;
}

void onStateChange(uint8_t state) {
  Serial.print("STATE ");
  Serial.println(stateName(state));
}

void onSignal(const zerokernel::Kernel::KernelSignal& signal) {
  Serial.print("SIGNAL ");
  Serial.print(signalName(signal.type));
  Serial.print(" label=");
  Serial.print(signal.label);
  Serial.print(" value=");
  Serial.println(signal.value);
}

void onWiFiState(const char*, const zerokernel::Kernel::EventValue& value) {
  Serial.print("WIFI status=");
  Serial.print(value.boolValue ? "connected" : "reconnecting");
  Serial.print(" ssid=");
  Serial.println(credentialsLookConfigured() ? kSsid : "not-configured");
}

void onTelemetrySample(const char*, const zerokernel::Kernel::EventValue& value) {
  Serial.print("SAMPLE seq=");
  Serial.print(gSampleSequence);
  Serial.print(" value=");
  Serial.println(value.unsignedValue);
}

void onHeartbeat(const char*, long value) {
  Serial.print("HEARTBEAT tick=");
  Serial.println(value);
}

void onDiagnosticsCommand(const char*, const zerokernel::Kernel::EventValue& value) {
  const bool enableDiagnostics =
      value.type == zerokernel::Kernel::kEventBool ? value.boolValue
                                                   : (value.longValue != 0);

  if (enableDiagnostics) {
    ZeroKernel.enableCapabilities(zerokernel::Kernel::kCapDiagnostics);
  } else {
    ZeroKernel.disableCapabilities(zerokernel::Kernel::kCapDiagnostics);
  }

  gDiagnosticsEnabled = enableDiagnostics;
  Serial.print("DIAG mode=");
  Serial.println(enableDiagnostics ? "enabled" : "paused");
}

void maintainWiFi() {
  const unsigned long nowMs = millis();

  if (WiFi.status() == WL_CONNECTED) {
    ZeroKernel.publishTypedFast(
        kWifiStateKey, zerokernel::Kernel::EventValue::fromBool(true));
    return;
  }

  if (credentialsLookConfigured() && (nowMs - gLastConnectAttemptMs) >= 4000UL) {
    gLastConnectAttemptMs = nowMs;
    WiFi.disconnect();
    WiFi.begin(kSsid, kPassword);
  }

  ZeroKernel.publishTypedFast(
      kWifiStateKey, zerokernel::Kernel::EventValue::fromBool(false));
}

void sampleSensors() {
  ++gSampleSequence;
  gLastSample = 200UL + ((millis() / 25UL) % 75UL);
}

void flushTelemetry() {
  ZeroKernel.publishDeferredTypedFast(
      kSampleKey, zerokernel::Kernel::EventValue::fromUnsigned(gLastSample));
}

void emitHeartbeat() {
  ++gHeartbeatCount;
  ZeroKernel.publishDeferredFast(kHeartbeatKey, static_cast<long>(gHeartbeatCount));
}

void rotateDiagnosticsMode() {
  const bool nextMode = !gDiagnosticsEnabled;
  ZeroKernel.enqueueCommandTypedFast(
      kDiagCommandKey, zerokernel::Kernel::EventValue::fromBool(nextMode));
}

void printRuntimeSummary() {
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();
  const zerokernel::Kernel::TimingReport timing = ZeroKernel.getTimingReport();

  Serial.print("STATS runs=");
  Serial.print(stats.taskExecutions);
  Serial.print(" events=");
  Serial.print(stats.eventsDelivered);
  Serial.print(" state=");
  Serial.print(stateName(ZeroKernel.state()));
  Serial.print(" wifi=");
  Serial.print(WiFi.status() == WL_CONNECTED ? "up" : "down");
  Serial.print(" max_tick_ms=");
  Serial.print(timing.worstTickDurationMs);
  Serial.print(" diag=");
  Serial.println(gDiagnosticsEnabled ? "on" : "off");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  WiFi.mode(WIFI_STA);

  ZeroKernel.begin(zerokernel::adapters::esp32MillisClock);
  ZeroKernel.setCapabilities(zerokernel::Kernel::kCapAll);
  ZeroKernel.setSafeModeCapabilities(zerokernel::Kernel::kCapTelemetry |
                                     zerokernel::Kernel::kCapDiagnostics |
                                     zerokernel::Kernel::kCapIO);
  ZeroKernel.setSignalHandler(onSignal);
  ZeroKernel.onStateChange(onStateChange);

  ZeroKernel.subscribeTypedFast(kWifiStateKey, onWiFiState, "telemetry.wifi");
  ZeroKernel.subscribeTypedFast(kSampleKey, onTelemetrySample, "telemetry.sample");
  ZeroKernel.subscribeFast(kHeartbeatKey, onHeartbeat, "telemetry.heartbeat");
  ZeroKernel.registerCommandFast(kDiagCommandKey,
                                 onDiagnosticsCommand,
                                 "telemetry.diag");

  zerokernel::Kernel::TaskConfig wifiTask = {
      "WiFiMaintainer",
      maintainWiFi,
      2000,
      25,
      0,
      zerokernel::Kernel::kPriorityHigh,
      true,
      {},
      zerokernel::Kernel::kCapIO};
  zerokernel::Kernel::TaskConfig sampleTask = {
      "SensorSampler",
      sampleSensors,
      250,
      5,
      0,
      zerokernel::Kernel::kPriorityNormal,
      true,
      {},
      zerokernel::Kernel::kCapIO};
  zerokernel::Kernel::TaskConfig telemetryTask = {
      "TelemetryFlush",
      flushTelemetry,
      1000,
      5,
      0,
      zerokernel::Kernel::kPriorityHigh,
      true,
      {},
      zerokernel::Kernel::kCapTelemetry};
  zerokernel::Kernel::TaskConfig heartbeatTask = {
      "Heartbeat",
      emitHeartbeat,
      3000,
      5,
      0,
      zerokernel::Kernel::kPriorityHigh,
      true,
      {},
      zerokernel::Kernel::kCapIO};
  zerokernel::Kernel::TaskConfig modeTask = {
      "DiagMode",
      rotateDiagnosticsMode,
      6000,
      5,
      0,
      zerokernel::Kernel::kPriorityLow,
      true,
      {},
      zerokernel::Kernel::kCapControl};
  zerokernel::Kernel::TaskConfig statsTask = {
      "StatsPrinter",
      printRuntimeSummary,
      5000,
      10,
      0,
      zerokernel::Kernel::kPriorityLow,
      true,
      {},
      zerokernel::Kernel::kCapDiagnostics};

  ZeroKernel.addTask(wifiTask);
  ZeroKernel.addTask(sampleTask);
  ZeroKernel.addTask(telemetryTask);
  ZeroKernel.addTask(heartbeatTask);
  ZeroKernel.addTask(modeTask);
  ZeroKernel.addTask(statsTask);
  ZeroKernel.setTaskHeartbeatTimeout("WiFiMaintainer", 3000);
}

void loop() {
  ZeroKernel.tick();
}
