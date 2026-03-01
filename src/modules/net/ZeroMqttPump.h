#ifndef ZEROKERNEL_MODULES_NET_ZEROMQTTPUMP_H
#define ZEROKERNEL_MODULES_NET_ZEROMQTTPUMP_H

#include "../../ZeroKernel.h"
#include "ZeroTransportMetrics.h"

namespace zerokernel {
namespace modules {
namespace net {

// Optional cooperative MQTT pump. Header-only and only linked when included.
// BETA: transport cadence is tuned, but release-grade validation is still in progress.
class ZeroMqttPump {
 public:
  struct Message {
    Kernel::TopicKey topicKey;
    Kernel::EventValue value;
    uint8_t retriesRemaining;
    unsigned long queuedAtMs;

    Message()
        : topicKey(0),
          value(Kernel::EventValue::fromLong(0)),
          retriesRemaining(0),
          queuedAtMs(0) {}
  };

  typedef bool (*LinkProbe)();
  typedef bool (*ConnectStep)(void* context);
  typedef void (*LoopStep)(void* context);
  typedef bool (*PublishStep)(Kernel::TopicKey topicKey,
                              const Kernel::EventValue& value,
                              void* context);

  struct Config {
    unsigned long pollIntervalMs;
    unsigned long retryBaseMs;
    unsigned long retryMaxMs;
    unsigned long retryJitterMs;
    uint8_t maxRetries;
    bool dropOldestOnFull;
    Kernel::TopicKey stateTopicKey;

    Config()
        : pollIntervalMs(50),
          retryBaseMs(500),
          retryMaxMs(5000),
          retryJitterMs(0),
          maxRetries(2),
          dropOldestOnFull(true),
          stateTopicKey(0) {}
  };

  static const uint8_t kQueueCapacity = ZEROKERNEL_MQTT_PUMP_QUEUE_CAPACITY;
  static const uint8_t kPublishBurstPerTick = 1;

  ZeroMqttPump()
      : kernel_(NULL),
        linkProbe_(NULL),
        connectStep_(NULL),
        loopStep_(NULL),
        publishStep_(NULL),
        context_(NULL),
        config_(),
        metrics_(),
        started_(false),
        hasTicked_(false),
        connected_(false),
        lastObservedConnected_(false),
        lastTickAtMs_(0),
        queueCount_(0),
        pendingRetryAtMs_(0),
        currentRetryMs_(0),
        retrySalt_(0) {}

  void begin(Kernel& kernel,
             LinkProbe linkProbe,
             ConnectStep connectStep,
             LoopStep loopStep,
             PublishStep publishStep,
             const Config& config = Config(),
             void* context = NULL) {
    kernel_ = &kernel;
    linkProbe_ = linkProbe;
    connectStep_ = connectStep;
    loopStep_ = loopStep;
    publishStep_ = publishStep;
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
    connected_ = false;
    lastObservedConnected_ = false;
    lastTickAtMs_ = 0;
    queueCount_ = 0;
    pendingRetryAtMs_ = 0;
    currentRetryMs_ = config_.retryBaseMs;
  }

  bool enqueue(Kernel::TopicKey topicKey,
               const Kernel::EventValue& value,
               uint8_t retriesRemaining = 0) {
    if (kernel_ == NULL) {
      return false;
    }

    Message message;
    message.topicKey = topicKey;
    message.value = value;
    message.retriesRemaining =
        retriesRemaining == 0 ? config_.maxRetries : retriesRemaining;
    message.queuedAtMs = kernel_->getStats().uptimeMs;

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

    queue_[queueCount_++] = message;
    metrics_.recordSendQueued(queueCount_);
    return true;
  }

  void tick() {
    if (!started_ || kernel_ == NULL || linkProbe_ == NULL || connectStep_ == NULL ||
        publishStep_ == NULL) {
      return;
    }

    const unsigned long nowMs = kernel_->getStats().uptimeMs;
    if (hasTicked_ && config_.pollIntervalMs > 0 &&
        (nowMs - lastTickAtMs_) < config_.pollIntervalMs) {
      return;
    }

    hasTicked_ = true;
    lastTickAtMs_ = nowMs;

    connected_ = linkProbe_();
    notifyStateIfChanged_();

    if (!connected_) {
      if (pendingRetryAtMs_ != 0 && nowMs < pendingRetryAtMs_) {
        return;
      }

      metrics_.recordConnectAttempt();
      if (connectStep_(context_)) {
        connected_ = true;
        pendingRetryAtMs_ = 0;
        currentRetryMs_ = config_.retryBaseMs;
        metrics_.recordConnectResult(true, 0);
        notifyStateIfChanged_();
      } else {
        metrics_.recordConnectResult(false, 0);
        scheduleBackoff_(nowMs);
      }
      return;
    }

    if (loopStep_ != NULL) {
      loopStep_(context_);
      metrics_.recordLoopCall();
    }

    if (pendingRetryAtMs_ != 0 && nowMs < pendingRetryAtMs_) {
      return;
    }

    if (queueCount_ == 0) {
      return;
    }

    pendingRetryAtMs_ = 0;
    for (uint8_t burst = 0; burst < kPublishBurstPerTick && queueCount_ > 0; ++burst) {
      metrics_.recordSendAttempt();
      if (publishStep_(queue_[0].topicKey, queue_[0].value, context_)) {
        metrics_.recordSendResult(true, 0, nowMs - queue_[0].queuedAtMs);
        popFront_();
        continue;
      }

      metrics_.recordSendResult(false, 0, nowMs - queue_[0].queuedAtMs);
      if (queue_[0].retriesRemaining == 0) {
        popFront_();
        return;
      }

      --queue_[0].retriesRemaining;
      scheduleBackoff_(nowMs);
      return;
    }
  }

  bool isConnected() const {
    return connected_;
  }

  uint8_t queuedCount() const {
    return queueCount_;
  }

  const ZeroTransportMetrics& metrics() const {
    return metrics_;
  }

 private:
  void notifyStateIfChanged_() {
    if (connected_ == lastObservedConnected_) {
      return;
    }

    lastObservedConnected_ = connected_;
    if (config_.stateTopicKey != 0) {
      kernel_->publishTypedFast(config_.stateTopicKey,
                                Kernel::EventValue::fromBool(connected_));
    }
  }

  void scheduleBackoff_(unsigned long nowMs) {
    if (currentRetryMs_ == 0) {
      currentRetryMs_ = config_.retryBaseMs;
    }

    pendingRetryAtMs_ = nowMs + currentRetryMs_ + computeRetryJitter_(nowMs);
    metrics_.recordBackoffSchedule();

    if (config_.retryMaxMs > 0 && currentRetryMs_ < config_.retryMaxMs) {
      const unsigned long doubled = currentRetryMs_ * 2UL;
      currentRetryMs_ = doubled > config_.retryMaxMs ? config_.retryMaxMs : doubled;
    }
  }

  void popFront_() {
    for (uint8_t index = 1; index < queueCount_; ++index) {
      queue_[index - 1] = queue_[index];
    }
    if (queueCount_ > 0) {
      --queueCount_;
    }
  }

  unsigned long computeRetryJitter_(unsigned long nowMs) {
    if (config_.retryJitterMs == 0) {
      return 0;
    }

    ++retrySalt_;
    return (nowMs + retrySalt_ + queueCount_) % (config_.retryJitterMs + 1UL);
  }

  Kernel* kernel_;
  LinkProbe linkProbe_;
  ConnectStep connectStep_;
  LoopStep loopStep_;
  PublishStep publishStep_;
  void* context_;
  Config config_;
  ZeroTransportMetrics metrics_;
  bool started_;
  bool hasTicked_;
  bool connected_;
  bool lastObservedConnected_;
  unsigned long lastTickAtMs_;
  Message queue_[kQueueCapacity];
  uint8_t queueCount_;
  unsigned long pendingRetryAtMs_;
  unsigned long currentRetryMs_;
  unsigned long retrySalt_;
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
