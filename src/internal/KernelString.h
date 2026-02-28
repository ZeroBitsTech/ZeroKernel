#ifndef ZEROKERNEL_INTERNAL_KERNELSTRING_H
#define ZEROKERNEL_INTERNAL_KERNELSTRING_H

#include <stddef.h>

namespace zerokernel {
namespace internal {

bool labelsEqual(const char* left, const char* right, size_t maxLength);
void copyLabel(char* destination, size_t length, const char* source);

}  // namespace internal
}  // namespace zerokernel

#endif
