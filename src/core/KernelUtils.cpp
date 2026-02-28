#include "../ZeroKernel.h"
#include "../internal/KernelHash.h"
#include "../internal/KernelString.h"

namespace zerokernel {

Kernel::TopicKey Kernel::makeTopicKey(const char* label) {
  return zk_fnv1a_32(label);
}

Kernel::TimeMs Kernel::currentTime_() const {
  if (clockSource_ == NULL) {
    return lastTickAtMs_;
  }

  return clockSource_();
}

void Kernel::copyLabel_(char* destination, size_t length, const char* source) const {
  internal::copyLabel(destination, length, source);
}

const char* Kernel::labelOrEmpty_(const char* source) const {
  return source != NULL ? source : "";
}

Kernel::TopicKey Kernel::topicKey_(const char* source) const {
  return makeTopicKey(source);
}

bool Kernel::eventValuesEqual_(const EventValue& left, const EventValue& right) const {
  if (left.type != right.type) {
    return false;
  }

  switch (left.type) {
    case kEventLong:
      return left.longValue == right.longValue;
    case kEventUnsigned:
      return left.unsignedValue == right.unsignedValue;
    case kEventBool:
      return left.boolValue == right.boolValue;
    case kEventPointer:
      return left.pointerValue == right.pointerValue;
    default:
      return false;
  }
}

void Kernel::resetTrace_() {
#if !ZEROKERNEL_ENABLE_TRACE
  traceHead_ = 0;
  traceTail_ = 0;
  traceCount_ = 0;
#else
  for (uint8_t i = 0; i < kTraceStorage; ++i) {
    traceBuffer_[i] = TraceEntry();
  }

  traceHead_ = 0;
  traceTail_ = 0;
  traceCount_ = 0;
#endif
}

void Kernel::pushTrace_(uint8_t type, const char* label, unsigned long value) {
#if !ZEROKERNEL_ENABLE_TRACE
  (void)type;
  (void)label;
  (void)value;
  return;
#else
  if (kMaxTraceEntries == 0) {
    return;
  }

  TraceEntry& entry = traceBuffer_[traceTail_];
  entry = TraceEntry();
  entry.timestampMs = currentTime_();
  entry.type = type;
  internal::copyLabel(entry.label, sizeof(entry.label), label);
  entry.value = value;

  if (traceCount_ == kMaxTraceEntries) {
    traceHead_ = static_cast<uint8_t>((traceHead_ + 1) % kTraceStorage);
  } else {
    ++traceCount_;
  }

  traceTail_ = static_cast<uint8_t>((traceTail_ + 1) % kTraceStorage);
#endif
}

void Kernel::emitSignal_(uint8_t type, const char* label, unsigned long value) {
  pushTrace_(type, label, value);

#if ZEROKERNEL_ENABLE_SIGNAL_HOOK
  if (signalHandler_ == NULL) {
    return;
  }

  KernelSignal signal = {};
  signal.type = type;
  internal::copyLabel(signal.label, sizeof(signal.label), label);
  signal.value = value;
  signalHandler_(signal);
#else
  (void)type;
  (void)label;
  (void)value;
#endif
}

}  // namespace zerokernel
