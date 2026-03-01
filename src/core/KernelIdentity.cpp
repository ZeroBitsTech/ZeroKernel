#include "../ZeroKernel.h"

namespace zerokernel {

namespace {

const Kernel::Identity kKernelIdentity = {
    "ZeroKernel",
    "ZeroBits",
    "Small Bits. Solid Systems.",
    "https://kernel.zerobits.tech",
    "1.4.1"};

}  // namespace

const Kernel::Identity& Kernel::identity() const {
  return kKernelIdentity;
}

uint16_t Kernel::abiVersion() const {
  return kAbiVersion;
}

const char* Kernel::runtimeVersion() const {
  return identity().version;
}

}  // namespace zerokernel
