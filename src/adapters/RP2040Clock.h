#ifndef ZEROKERNEL_ADAPTERS_RP2040CLOCK_H
#define ZEROKERNEL_ADAPTERS_RP2040CLOCK_H

#include "../ZeroKernel.h"

#if defined(ARDUINO) && (defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_MBED_RP2040))
#include <Arduino.h>
#endif

namespace zerokernel {
namespace adapters {

#if defined(ARDUINO) && (defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_MBED_RP2040))
inline Kernel::TimeMs rp2040MillisClock() {
  return millis();
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
