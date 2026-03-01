#ifndef ZEROKERNEL_MODULES_NET_ZERONETPROFILEESP8266HTTP_H
#define ZEROKERNEL_MODULES_NET_ZERONETPROFILEESP8266HTTP_H

#include "ZeroHttpPump.h"
#include "ZeroMqttPump.h"
#include "ZeroWiFiMaintainer.h"

namespace zerokernel {
namespace modules {
namespace net {

// Recommended constrained HTTP-first preset for ESP8266-class boards.
// This profile keeps HTTP alive without forcing MQTT churn on a board that is
// already sensitive to transport wake-ups.
struct ZeroNetProfileEsp8266Http {
  static const uint8_t kRecommendedIdleStrategy = Kernel::kIdleYield;
  static const bool kEnableHttpByDefault = true;
  static const bool kEnableMqttByDefault = false;
  static const unsigned long kSampleTaskIntervalMs = 100UL;
  static const unsigned long kWiFiTaskIntervalMs = 250UL;
  static const unsigned long kHttpTaskIntervalMs = 100UL;
  static const unsigned long kMqttTaskIntervalMs = 500UL;
  static const unsigned long kDispatchTaskIntervalMs = 250UL;
  static const unsigned long kReportTaskIntervalMs = 1000UL;

  static const unsigned long kWiFiTaskStartDelayMs = 25UL;
  static const unsigned long kHttpTaskStartDelayMs = 75UL;
  static const unsigned long kMqttTaskStartDelayMs = 125UL;
  static const unsigned long kDispatchTaskStartDelayMs = 175UL;

  static const unsigned long kHttpDispatchPeriodMs = 1500UL;
  static const unsigned long kMqttDispatchPeriodMs = 0UL;
  static const unsigned long kHttpIoTimeoutMs = 450UL;
  static const uint8_t kHttpStablePollsRequired = 1;

  static ZeroWiFiMaintainer::Config wifiConfig() {
    ZeroWiFiMaintainer::Config config;
    config.pollIntervalMs = 500UL;
    config.retryBaseMs = 2000UL;
    config.retryMaxMs = 8000UL;
    config.retryJitterMs = 250UL;
    config.stablePollMultiplier = 4;
    config.stableThreshold = 6;
    return config;
  }

  static ZeroHttpPump::Config httpConfig() {
    ZeroHttpPump::Config config;
    config.pollIntervalMs = 100UL;
    config.retryBaseMs = 700UL;
    config.retryMaxMs = 3200UL;
    config.retryJitterMs = 180UL;
    config.phaseTimeoutMs = 300UL;
    config.connectPhaseTimeoutMs = 450UL;
    config.writePhaseTimeoutMs = 100UL;
    config.readPhaseTimeoutMs = 650UL;
    config.closePhaseTimeoutMs = 75UL;
    config.maxRetries = 1;
    config.immediatePhaseBudget = 2;
    config.queueWhenLinkDown = false;
    return config;
  }

  static ZeroMqttPump::Config mqttConfig() {
    ZeroMqttPump::Config config;
    config.pollIntervalMs = 100UL;
    config.retryBaseMs = 600UL;
    config.retryMaxMs = 3000UL;
    config.retryJitterMs = 180UL;
    config.idleLoopIntervalMs = 500UL;
    config.maxRetries = 2;
    config.queueWhenTransportDown = false;
    return config;
  }
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
