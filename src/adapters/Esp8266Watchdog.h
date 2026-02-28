#ifndef ZEROKERNEL_ADAPTERS_ESP8266WATCHDOG_H
#define ZEROKERNEL_ADAPTERS_ESP8266WATCHDOG_H

#include "../ZeroKernel.h"

#if defined(ARDUINO) && defined(ESP8266)
#include <Arduino.h>
#endif

namespace zerokernel {
namespace adapters {

#if defined(ARDUINO) && defined(ESP8266)
inline void esp8266FeedHardwareWatchdog() {
  ESP.wdtFeed();
}

inline Kernel::HardwareWatchdogBridge esp8266WatchdogBridge() {
  Kernel::HardwareWatchdogBridge bridge = {};
  bridge.feed = esp8266FeedHardwareWatchdog;
  bridge.feedOnTick = true;
  bridge.feedAfterTask = true;
  return bridge;
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
