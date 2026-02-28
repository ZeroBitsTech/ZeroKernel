#include "KernelHash.h"

uint32_t zk_fnv1a_32(const char* label) {
  if (label == 0) {
    return 0U;
  }

  uint32_t hash = 2166136261UL;

  while (*label != '\0') {
    hash ^= (uint8_t)(*label);
    hash *= 16777619UL;
    ++label;
  }

  return hash == 0U ? 1U : hash;
}
