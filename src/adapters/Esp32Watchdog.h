#ifndef ZEROKERNEL_ADAPTERS_ESP32WATCHDOG_H
#define ZEROKERNEL_ADAPTERS_ESP32WATCHDOG_H

#include "../ZeroKernel.h"

#if defined(ARDUINO) && defined(ESP32)
#include <esp_task_wdt.h>
#endif

namespace zerokernel {
namespace adapters {

#if defined(ARDUINO) && defined(ESP32)
inline void esp32FeedHardwareWatchdog() {
  esp_task_wdt_reset();
}

inline Kernel::HardwareWatchdogBridge esp32WatchdogBridge() {
  Kernel::HardwareWatchdogBridge bridge = {};
  bridge.feed = esp32FeedHardwareWatchdog;
  bridge.feedOnTick = true;
  bridge.feedAfterTask = true;
  return bridge;
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
