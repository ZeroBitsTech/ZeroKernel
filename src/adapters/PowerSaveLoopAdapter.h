#ifndef ZEROKERNEL_ADAPTERS_POWERSAVELOOPADAPTER_H
#define ZEROKERNEL_ADAPTERS_POWERSAVELOOPADAPTER_H

#include "../ZeroKernel.h"
#include "../internal/KernelArch.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace zerokernel {
namespace adapters {

inline void idleHint() {
  internal::idleCpu();

#if defined(ARDUINO) && !defined(__arm__) && !defined(__thumb__) && !defined(__riscv) && \
    !defined(__XTENSA__)
  yield();
#endif
}

inline void powerSaveTick(Kernel& kernel) {
  kernel.tick();

#if defined(ARDUINO)
  const uint8_t idleStrategy = kernel.getIdleStrategy();
  if (idleStrategy == Kernel::kIdleBusy) {
    return;
  }

  const Kernel::TimeMs sleepMs = kernel.nextWakeInMs();
  if (idleStrategy == Kernel::kIdleSleep && sleepMs > 1) {
    delay(sleepMs - 1);
  } else if (idleStrategy == Kernel::kIdleYield) {
    yield();
  } else {
    idleHint();
  }
#endif
}

}  // namespace adapters
}  // namespace zerokernel

#endif
