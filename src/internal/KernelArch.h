#ifndef ZEROKERNEL_INTERNAL_KERNELARCH_H
#define ZEROKERNEL_INTERNAL_KERNELARCH_H

#include <stdint.h>

#include "KernelArchAsm.h"

namespace zerokernel {
namespace internal {

inline void idleCpu() {
  zk_arch_idle_hint_asm();
}

inline uint32_t atomicLoadU32(const volatile uint32_t* target) {
#if defined(__GNUC__) || defined(__clang__)
  return __atomic_load_n(target, __ATOMIC_RELAXED);
#else
  return *target;
#endif
}

inline void atomicStoreU32(volatile uint32_t* target, uint32_t value) {
#if defined(__GNUC__) || defined(__clang__)
  __atomic_store_n(target, value, __ATOMIC_RELAXED);
#else
  *target = value;
#endif
}

inline uint32_t atomicOrU32(volatile uint32_t* target, uint32_t mask) {
#if defined(__GNUC__) || defined(__clang__)
  return __atomic_fetch_or(target, mask, __ATOMIC_RELAXED);
#else
  const uint32_t previous = *target;
  *target = static_cast<uint32_t>(previous | mask);
  return previous;
#endif
}

inline uint32_t atomicAndU32(volatile uint32_t* target, uint32_t mask) {
#if defined(__GNUC__) || defined(__clang__)
  return __atomic_fetch_and(target, mask, __ATOMIC_RELAXED);
#else
  const uint32_t previous = *target;
  *target = static_cast<uint32_t>(previous & mask);
  return previous;
#endif
}

inline bool hasCycleCounter() {
#if defined(__XTENSA__)
  return true;
#else
  return false;
#endif
}

inline uint32_t readCycleCounter() {
#if defined(__XTENSA__)
  uint32_t value = 0;
  __asm__ __volatile__("rsr.ccount %0" : "=a"(value));
  return value;
#else
  return 0;
#endif
}

}  // namespace internal
}  // namespace zerokernel

#endif
