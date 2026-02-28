#include <ZeroKernel.h>
#include <adapters/PowerSaveLoopAdapter.h>
#include <modules/net/ZeroWiFiMaintainer.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#else
#error "NetworkMaintainer example requires ESP8266 or ESP32."
#endif

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroWiFiMaintainer;

namespace {

const char* kSsid = "replace-me";
const char* kPassword = "replace-me";
const Kernel::TopicKey kWiFiStateTopic = Kernel::makeTopicKey("wifi.link");

ZeroWiFiMaintainer g_wifiMaintainer;

unsigned long boardMillis() {
  return millis();
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void connectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(kSsid, kPassword);
  }
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

void onWiFiState(const char*, const Kernel::EventValue& value) {
  if (value.type != Kernel::kEventBool) {
    return;
  }

  Serial.print("wifi.state => ");
  Serial.println(value.boolValue ? "connected" : "disconnected");
}

void heartbeatTask() {
  const Kernel::KernelStats stats = ZeroKernel.getStats();

  Serial.print("heartbeat runs=");
  Serial.print(stats.taskExecutions);
  Serial.print(" events=");
  Serial.print(stats.eventsDelivered);
  Serial.print(" wifi_attempts=");
  Serial.print(g_wifiMaintainer.connectAttempts());
  Serial.print(" reconnects=");
  Serial.println(g_wifiMaintainer.reconnectTransitions());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  ZeroKernel.begin(boardMillis);
  ZeroKernel.setIdleStrategy(zerokernel::Kernel::kIdleSleep);
  ZeroKernel.subscribeTypedFast(kWiFiStateTopic, onWiFiState);

  ZeroWiFiMaintainer::Config config;
  config.pollIntervalMs = 250;
  config.retryBaseMs = 1000;
  config.retryMaxMs = 15000;
  config.manageCapabilities = true;
  config.capabilityMask = Kernel::kCapNetwork;
  config.stateTopicKey = kWiFiStateTopic;

  g_wifiMaintainer.begin(ZeroKernel, isWiFiConnected, connectWiFi, disconnectWiFi, config);

  ZeroKernel.addTask("Heartbeat", heartbeatTask, 1000, 0);
  ZeroKernel.setTaskPriority("Heartbeat", Kernel::kPriorityLow);
}

void loop() {
  zerokernel::adapters::powerSaveTick(ZeroKernel);
  g_wifiMaintainer.tick();
}
