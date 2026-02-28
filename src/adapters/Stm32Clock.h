#ifndef ZEROKERNEL_ADAPTERS_STM32CLOCK_H
#define ZEROKERNEL_ADAPTERS_STM32CLOCK_H

#include "../ZeroKernel.h"

namespace zerokernel {
namespace adapters {

#if defined(STM32F1xx) || defined(STM32F4xx) || defined(STM32F7xx) || defined(STM32H7xx) || \
    defined(STM32G0xx) || defined(STM32G4xx) || defined(STM32L4xx)
extern "C" unsigned long HAL_GetTick(void);

inline Kernel::TimeMs stm32HalClock() {
  return static_cast<Kernel::TimeMs>(HAL_GetTick());
}
#endif

}  // namespace adapters
}  // namespace zerokernel

#endif
