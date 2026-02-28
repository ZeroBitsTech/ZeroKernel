#ifndef ZEROKERNEL_ADAPTERS_ESP32CLOCK_H
#define ZEROKERNEL_ADAPTERS_ESP32CLOCK_H

#include "../ZeroKernel.h"

#if defined(ARDUINO) && defined(ESP32)
#include <Arduino.h>
#endif

namespace zerokernel {
namespace adapters {

#if defined(ARDUINO) && defined(ESP32)
inline Kernel::TimeMs esp32MillisClock() {
  return millis();
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
