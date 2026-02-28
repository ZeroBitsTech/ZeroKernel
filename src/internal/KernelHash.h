#ifndef ZEROKERNEL_INTERNAL_KERNELHASH_H
#define ZEROKERNEL_INTERNAL_KERNELHASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t zk_fnv1a_32(const char* label);

#ifdef __cplusplus
}
#endif

#endif
