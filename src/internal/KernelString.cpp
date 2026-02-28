#include "KernelString.h"

namespace zerokernel {
namespace internal {

bool labelsEqual(const char* left, const char* right, size_t maxLength) {
  if (left == NULL || right == NULL) {
    return false;
  }

  for (size_t index = 0; index < maxLength; ++index) {
    if (left[index] != right[index]) {
      return false;
    }

    if (left[index] == '\0') {
      return true;
    }
  }

  return true;
}

void copyLabel(char* destination, size_t length, const char* source) {
  if (destination == NULL || length == 0) {
    return;
  }

  destination[0] = '\0';
  if (source == NULL) {
    return;
  }

  size_t index = 0;
  while ((index + 1) < length && source[index] != '\0') {
    destination[index] = source[index];
    ++index;
  }

  destination[index] = '\0';
}

}  // namespace internal
}  // namespace zerokernel
