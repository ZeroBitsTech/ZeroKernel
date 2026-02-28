#include <ESP8266WiFi.h>
#include <ZeroKernel.h>
#include <adapters/ArduinoClock.h>

const char* kSsid = "replace-with-ssid";
const char* kPassword = "replace-with-password";

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    ZeroKernel.publishTyped("wifi.connected",
                            zerokernel::Kernel::EventValue::fromBool(true));
    return;
  }

  WiFi.disconnect();
  WiFi.begin(kSsid, kPassword);
  ZeroKernel.publishTyped("wifi.connected",
                          zerokernel::Kernel::EventValue::fromBool(false));
}

void onWiFiState(const char* topic, const zerokernel::Kernel::EventValue& value) {
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(value.boolValue ? "connected" : "reconnecting");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  ZeroKernel.begin(zerokernel::adapters::arduinoMillisClock);
  ZeroKernel.subscribeTyped("wifi.connected", onWiFiState);
  ZeroKernel.addTask("WiFiMaintainer", maintainWiFi, 5000, 50);
  ZeroKernel.setTaskHeartbeatTimeout("WiFiMaintainer", 2000);
}

void loop() {
  ZeroKernel.tick();
}
