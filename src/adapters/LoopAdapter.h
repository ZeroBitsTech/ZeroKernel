#ifndef ZEROKERNEL_ADAPTERS_LOOPADAPTER_H
#define ZEROKERNEL_ADAPTERS_LOOPADAPTER_H

#include "../ZeroKernel.h"

namespace zerokernel {
namespace adapters {

class LoopAdapter {
 public:
  explicit LoopAdapter(Kernel& kernel) : kernel_(kernel) {}

  void begin(Kernel::ClockSource clockSource) {
    kernel_.begin(clockSource);
  }

  void poll() {
    kernel_.tick();
  }

  void poll(Kernel::TimeMs nowMs) {
    kernel_.tick(nowMs);
  }

 private:
  Kernel& kernel_;
};

}  // namespace adapters
}  // namespace zerokernel

#endif
