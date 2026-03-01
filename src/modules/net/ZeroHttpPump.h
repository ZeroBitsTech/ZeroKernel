#ifndef ZEROKERNEL_MODULES_NET_ZEROHTTPPUMP_H
#define ZEROKERNEL_MODULES_NET_ZEROHTTPPUMP_H

#include "../../ZeroKernel.h"
#include "ZeroTransportMetrics.h"

namespace zerokernel {
namespace modules {
namespace net {

// Optional cooperative HTTP request pump. Header-only and only linked when included.
// BETA: suitable for evaluation, but still under active transport and retry tuning.
class ZeroHttpPump {
 public:
  enum StepResult : int8_t {
    kStepFailed = -1,
    kStepPending = 0,
    kStepComplete = 1
  };

  enum Phase : uint8_t {
    kPhaseIdle = 0,
    kPhaseConnecting = 1,
    kPhaseWriting = 2,
    kPhaseReading = 3,
    kPhaseClosing = 4
  };

  struct Request {
    const char* path;
    const char* contentType;
    const char* body;
    uint16_t bodyLength;
    uint8_t retriesRemaining;
    unsigned long queuedAtMs;
    Kernel::TopicKey completionTopicKey;

    Request()
        : path(NULL),
          contentType(NULL),
          body(NULL),
          bodyLength(0),
          retriesRemaining(0),
          queuedAtMs(0),
          completionTopicKey(0) {}
  };

  typedef StepResult (*StepCallback)(const Request& request, void* context);

  struct Config {
    unsigned long pollIntervalMs;
    unsigned long retryBaseMs;
    unsigned long retryMaxMs;
    unsigned long retryJitterMs;
    uint8_t maxRetries;
    bool dropOldestOnFull;
    bool emitCompletionEvents;

    Config()
        : pollIntervalMs(0),
          retryBaseMs(250),
          retryMaxMs(2000),
          retryJitterMs(0),
          maxRetries(2),
          dropOldestOnFull(true),
          emitCompletionEvents(true) {}
  };

  static const uint8_t kQueueCapacity = ZEROKERNEL_HTTP_PUMP_QUEUE_CAPACITY;
  static const uint8_t kImmediatePhaseBudget = 4;

  ZeroHttpPump()
      : kernel_(NULL),
        connectStep_(NULL),
        writeStep_(NULL),
        readStep_(NULL),
        closeStep_(NULL),
        context_(NULL),
        config_(),
        metrics_(),
        started_(false),
        hasTicked_(false),
        active_(false),
        phase_(kPhaseIdle),
        queueCount_(0),
        activeRetriesRemaining_(0),
        lastTickAtMs_(0),
        phaseStartedAtMs_(0),
        activeStartedAtMs_(0),
        nextRetryAtMs_(0),
        currentRetryMs_(0),
        retrySalt_(0) {}

  void begin(Kernel& kernel,
             StepCallback connectStep,
             StepCallback writeStep,
             StepCallback readStep,
             StepCallback closeStep = NULL,
             const Config& config = Config(),
             void* context = NULL) {
    kernel_ = &kernel;
    connectStep_ = connectStep;
    writeStep_ = writeStep;
    readStep_ = readStep;
    closeStep_ = closeStep;
    context_ = context;
    config_ = config;
    reset();
    started_ = true;
    currentRetryMs_ = config.retryBaseMs;
  }

  void reset() {
    metrics_.reset();
    started_ = false;
    hasTicked_ = false;
    active_ = false;
    phase_ = kPhaseIdle;
    queueCount_ = 0;
    activeRetriesRemaining_ = 0;
    lastTickAtMs_ = 0;
    phaseStartedAtMs_ = 0;
    activeStartedAtMs_ = 0;
    nextRetryAtMs_ = 0;
    currentRetryMs_ = config_.retryBaseMs;
  }

  bool enqueue(const Request& request) {
    if (kernel_ == NULL) {
      return false;
    }

    Request queued = request;
    if (queued.retriesRemaining == 0) {
      queued.retriesRemaining = config_.maxRetries;
    }
    queued.queuedAtMs = kernel_->getStats().uptimeMs;

    if (queueCount_ >= kQueueCapacity) {
      if (!config_.dropOldestOnFull) {
        metrics_.recordQueueDrop();
        return false;
      }

      for (uint8_t index = 1; index < queueCount_; ++index) {
        queue_[index - 1] = queue_[index];
      }
      --queueCount_;
      metrics_.recordQueueDrop();
    }

    queue_[queueCount_++] = queued;
    metrics_.recordSendQueued(queueCount_);
    return true;
  }

  void tick() {
    if (!started_ || kernel_ == NULL || connectStep_ == NULL || writeStep_ == NULL ||
        readStep_ == NULL) {
      return;
    }

    const unsigned long nowMs = kernel_->getStats().uptimeMs;
    if (hasTicked_ && config_.pollIntervalMs > 0 &&
        (nowMs - lastTickAtMs_) < config_.pollIntervalMs) {
      return;
    }

    hasTicked_ = true;
    lastTickAtMs_ = nowMs;

    for (uint8_t stepBudget = 0; stepBudget < kImmediatePhaseBudget; ++stepBudget) {
      if (!active_) {
        if (queueCount_ == 0) {
          return;
        }

        startNextRequest_(nowMs);
      }

      if (phase_ == kPhaseConnecting && nextRetryAtMs_ != 0 && nowMs < nextRetryAtMs_) {
        return;
      }

      const StepResult result = stepCurrentPhase_();
      if (result == kStepPending) {
        return;
      }

      if (result == kStepFailed) {
        handlePhaseFailure_(nowMs);
        return;
      }

      handlePhaseSuccess_(nowMs);
      if (!active_) {
        return;
      }
    }
  }

  bool isBusy() const {
    return active_;
  }

  uint8_t queuedCount() const {
    return queueCount_;
  }

  uint8_t phase() const {
    return phase_;
  }

  const ZeroTransportMetrics& metrics() const {
    return metrics_;
  }

 private:
  void startNextRequest_(unsigned long nowMs) {
    activeRequest_ = queue_[0];
    for (uint8_t index = 1; index < queueCount_; ++index) {
      queue_[index - 1] = queue_[index];
    }
    --queueCount_;

    active_ = true;
    phase_ = kPhaseConnecting;
    activeRetriesRemaining_ = activeRequest_.retriesRemaining;
    phaseStartedAtMs_ = nowMs;
    activeStartedAtMs_ = nowMs;
    nextRetryAtMs_ = 0;
    currentRetryMs_ = config_.retryBaseMs;
    metrics_.recordConnectAttempt();
  }

  StepResult stepCurrentPhase_() {
    if (phase_ == kPhaseConnecting) {
      return connectStep_(activeRequest_, context_);
    }

    if (phase_ == kPhaseWriting) {
      return writeStep_(activeRequest_, context_);
    }

    if (phase_ == kPhaseReading) {
      return readStep_(activeRequest_, context_);
    }

    if (phase_ == kPhaseClosing && closeStep_ != NULL) {
      return closeStep_(activeRequest_, context_);
    }

    return kStepComplete;
  }

  void handlePhaseSuccess_(unsigned long nowMs) {
    const unsigned long phaseLatencyMs = nowMs - phaseStartedAtMs_;

    if (phase_ == kPhaseConnecting) {
      metrics_.recordConnectResult(true, phaseLatencyMs);
      metrics_.recordSendAttempt();
      phase_ = kPhaseWriting;
      phaseStartedAtMs_ = nowMs;
      nextRetryAtMs_ = 0;
      return;
    }

    if (phase_ == kPhaseWriting) {
      phase_ = kPhaseReading;
      phaseStartedAtMs_ = nowMs;
      return;
    }

    if (phase_ == kPhaseReading && closeStep_ != NULL) {
      phase_ = kPhaseClosing;
      phaseStartedAtMs_ = nowMs;
      return;
    }

    finalizeSuccess_(nowMs);
  }

  void handlePhaseFailure_(unsigned long nowMs) {
    const unsigned long phaseLatencyMs = nowMs - phaseStartedAtMs_;

    if (phase_ == kPhaseConnecting) {
      metrics_.recordConnectResult(false, phaseLatencyMs);
    } else {
      metrics_.recordSendResult(false, nowMs - activeStartedAtMs_, nowMs - activeRequest_.queuedAtMs);
    }

    if (closeStep_ != NULL && phase_ != kPhaseClosing) {
      closeStep_(activeRequest_, context_);
    }

    if (activeRetriesRemaining_ == 0) {
      finalizeFailure_(nowMs);
      return;
    }

    --activeRetriesRemaining_;
    if (currentRetryMs_ == 0) {
      currentRetryMs_ = config_.retryBaseMs;
    }

    nextRetryAtMs_ = nowMs + currentRetryMs_ + computeRetryJitter_(nowMs);
    metrics_.recordBackoffSchedule();

    if (config_.retryMaxMs > 0 && currentRetryMs_ < config_.retryMaxMs) {
      const unsigned long doubled = currentRetryMs_ * 2UL;
      currentRetryMs_ = doubled > config_.retryMaxMs ? config_.retryMaxMs : doubled;
    }

    phase_ = kPhaseConnecting;
    phaseStartedAtMs_ = nowMs;
    metrics_.recordConnectAttempt();
  }

  void finalizeSuccess_(unsigned long nowMs) {
    metrics_.recordSendResult(true, nowMs - activeStartedAtMs_, nowMs - activeRequest_.queuedAtMs);
    emitCompletion_(true);
    clearActive_();
  }

  void finalizeFailure_(unsigned long) {
    emitCompletion_(false);
    clearActive_();
  }

  void emitCompletion_(bool success) {
    if (!config_.emitCompletionEvents || activeRequest_.completionTopicKey == 0 || kernel_ == NULL) {
      return;
    }

    kernel_->publishTypedFast(activeRequest_.completionTopicKey,
                              Kernel::EventValue::fromBool(success));
  }

  void clearActive_() {
    active_ = false;
    phase_ = kPhaseIdle;
    activeRetriesRemaining_ = 0;
    phaseStartedAtMs_ = 0;
    activeStartedAtMs_ = 0;
    nextRetryAtMs_ = 0;
    currentRetryMs_ = config_.retryBaseMs;
    activeRequest_ = Request();
  }

  unsigned long computeRetryJitter_(unsigned long nowMs) {
    if (config_.retryJitterMs == 0) {
      return 0;
    }

    ++retrySalt_;
    return (nowMs + retrySalt_ + activeRequest_.queuedAtMs) % (config_.retryJitterMs + 1UL);
  }

  Kernel* kernel_;
  StepCallback connectStep_;
  StepCallback writeStep_;
  StepCallback readStep_;
  StepCallback closeStep_;
  void* context_;
  Config config_;
  ZeroTransportMetrics metrics_;
  bool started_;
  bool hasTicked_;
  bool active_;
  uint8_t phase_;
  Request queue_[kQueueCapacity];
  Request activeRequest_;
  uint8_t queueCount_;
  uint8_t activeRetriesRemaining_;
  unsigned long lastTickAtMs_;
  unsigned long phaseStartedAtMs_;
  unsigned long activeStartedAtMs_;
  unsigned long nextRetryAtMs_;
  unsigned long currentRetryMs_;
  unsigned long retrySalt_;
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
