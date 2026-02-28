#ifndef ZEROKERNEL_ADAPTERS_ARDUINOCLOCK_H
#define ZEROKERNEL_ADAPTERS_ARDUINOCLOCK_H

#include "../ZeroKernel.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace zerokernel {
namespace adapters {

#if defined(ARDUINO)
inline Kernel::TimeMs arduinoMillisClock() {
  return millis();
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
