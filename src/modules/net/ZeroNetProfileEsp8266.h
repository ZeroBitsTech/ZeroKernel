#ifndef ZEROKERNEL_MODULES_NET_ZERONETPROFILEESP8266_H
#define ZEROKERNEL_MODULES_NET_ZERONETPROFILEESP8266_H

#include "ZeroHttpPump.h"
#include "ZeroMqttPump.h"
#include "ZeroWiFiMaintainer.h"

namespace zerokernel {
namespace modules {
namespace net {

// Recommended constrained-network preset for ESP8266-class boards.
// This profile intentionally prefers MQTT cadence over aggressive HTTP churn.
struct ZeroNetProfileEsp8266 {
  static const uint8_t kRecommendedIdleStrategy = Kernel::kIdleYield;
  static const unsigned long kSampleTaskIntervalMs = 100UL;
  static const unsigned long kWiFiTaskIntervalMs = 250UL;
  static const unsigned long kHttpTaskIntervalMs = 250UL;
  static const unsigned long kMqttTaskIntervalMs = 250UL;
  static const unsigned long kDispatchTaskIntervalMs = 250UL;
  static const unsigned long kReportTaskIntervalMs = 250UL;

  static const unsigned long kWiFiTaskStartDelayMs = 25UL;
  static const unsigned long kHttpTaskStartDelayMs = 75UL;
  static const unsigned long kMqttTaskStartDelayMs = 125UL;
  static const unsigned long kDispatchTaskStartDelayMs = 175UL;

  static const unsigned long kHttpDispatchPeriodMs = 0UL;
  static const unsigned long kMqttDispatchPeriodMs = 1000UL;
  static const unsigned long kHttpIoTimeoutMs = 200UL;

  static ZeroWiFiMaintainer::Config wifiConfig() {
    ZeroWiFiMaintainer::Config config;
    config.pollIntervalMs = 1000UL;
    config.retryBaseMs = 4000UL;
    config.retryMaxMs = 12000UL;
    config.retryJitterMs = 350UL;
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
    config.maxRetries = 2;
    config.queueWhenLinkDown = false;
    return config;
  }

  static ZeroMqttPump::Config mqttConfig() {
    ZeroMqttPump::Config config;
    config.pollIntervalMs = 100UL;
    config.retryBaseMs = 600UL;
    config.retryMaxMs = 3000UL;
    config.retryJitterMs = 180UL;
    config.idleLoopIntervalMs = 150UL;
    config.maxRetries = 2;
    config.queueWhenTransportDown = false;
    return config;
  }
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
