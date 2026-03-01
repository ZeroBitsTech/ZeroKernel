#ifndef ZEROKERNEL_MODULES_NET_ZEROWIFIMAINTAINER_H
#define ZEROKERNEL_MODULES_NET_ZEROWIFIMAINTAINER_H

#include "../../ZeroKernel.h"

namespace zerokernel {
namespace modules {
namespace net {

// Optional Wi-Fi link helper. No runtime cost unless this module is included.
// BETA: suitable for live evaluation, but still under reconnect and long-run soak validation.
class ZeroWiFiMaintainer {
 public:
  typedef bool (*LinkProbe)();
  typedef void (*ConnectStep)();
  typedef void (*DisconnectStep)();
  typedef void (*StateWriter)(bool connected, unsigned long nowMs);

  struct Config {
    unsigned long pollIntervalMs;
    unsigned long retryBaseMs;
    unsigned long retryMaxMs;
    unsigned long retryJitterMs;
    uint8_t stablePollMultiplier;
    uint8_t stableThreshold;
    bool emitStateChangesOnly;
    bool manageCapabilities;
    Kernel::CapabilityMask capabilityMask;
    Kernel::TopicKey stateTopicKey;

    Config()
        : pollIntervalMs(500),
#if defined(ARDUINO_ARCH_ESP8266)
          retryBaseMs(3000),
          retryMaxMs(15000),
          retryJitterMs(500),
#else
          retryBaseMs(1000),
          retryMaxMs(10000),
          retryJitterMs(300),
#endif
          stablePollMultiplier(4),
          stableThreshold(6),
          emitStateChangesOnly(true),
          manageCapabilities(false),
          capabilityMask(Kernel::kCapNetwork),
          stateTopicKey(0) {}
  };

  ZeroWiFiMaintainer()
      : kernel_(NULL),
        probe_(NULL),
        connectStep_(NULL),
        disconnectStep_(NULL),
        stateWriter_(NULL),
        config_(),
        started_(false),
        hasPolled_(false),
        connected_(false),
        lastObservedConnected_(false),
        lastPollAtMs_(0),
        nextRetryAtMs_(0),
        currentRetryMs_(0),
        connectAttempts_(0),
        reconnectTransitions_(0),
        stateNotifications_(0),
        consecutiveStablePolls_(0) {}

  void begin(Kernel& kernel,
             LinkProbe probe,
             ConnectStep connectStep,
             DisconnectStep disconnectStep,
             const Config& config = Config(),
             StateWriter stateWriter = NULL) {
    kernel_ = &kernel;
    probe_ = probe;
    connectStep_ = connectStep;
    disconnectStep_ = disconnectStep;
    stateWriter_ = stateWriter;
    config_ = config;
    started_ = true;
    hasPolled_ = false;
    connected_ = false;
    lastObservedConnected_ = false;
    lastPollAtMs_ = 0;
    nextRetryAtMs_ = 0;
    currentRetryMs_ = config.retryBaseMs;
    connectAttempts_ = 0;
    reconnectTransitions_ = 0;
    stateNotifications_ = 0;
  }

  void reset() {
    started_ = false;
    hasPolled_ = false;
    connected_ = false;
    lastObservedConnected_ = false;
    lastPollAtMs_ = 0;
    nextRetryAtMs_ = 0;
    currentRetryMs_ = 0;
    connectAttempts_ = 0;
    reconnectTransitions_ = 0;
    stateNotifications_ = 0;
    consecutiveStablePolls_ = 0;
  }

  void tick() {
    if (!started_ || kernel_ == NULL || probe_ == NULL) {
      return;
    }

    const unsigned long nowMs = kernel_->getStats().uptimeMs;
    unsigned long effectivePollMs = config_.pollIntervalMs;
    if (config_.stablePollMultiplier > 1 &&
        consecutiveStablePolls_ >= config_.stableThreshold) {
      effectivePollMs *= config_.stablePollMultiplier;
    }
    if (hasPolled_ && (nowMs - lastPollAtMs_) < effectivePollMs) {
      return;
    }

    hasPolled_ = true;
    lastPollAtMs_ = nowMs;
    const bool linkUp = probe_();

    if (linkUp) {
      if (!connected_) {
        ++reconnectTransitions_;
        consecutiveStablePolls_ = 0;
      } else if (consecutiveStablePolls_ < 255) {
        ++consecutiveStablePolls_;
      }
      connected_ = true;
      currentRetryMs_ = config_.retryBaseMs;
      nextRetryAtMs_ = 0;
      applyNetworkCapabilities_(true);
      notifyState_(true, nowMs);
      return;
    }

    if (connected_) {
      connected_ = false;
      consecutiveStablePolls_ = 0;
      if (disconnectStep_ != NULL) {
        disconnectStep_();
      }
    }

    applyNetworkCapabilities_(false);
    notifyState_(false, nowMs);

    if (connectStep_ == NULL) {
      return;
    }

    if (nextRetryAtMs_ != 0 && nowMs < nextRetryAtMs_) {
      return;
    }

    connectStep_();
    ++connectAttempts_;

    if (currentRetryMs_ == 0) {
      currentRetryMs_ = config_.retryBaseMs;
    }

    nextRetryAtMs_ = nowMs + currentRetryMs_ + computeRetryJitter_(nowMs);

    if (config_.retryMaxMs > 0 && currentRetryMs_ < config_.retryMaxMs) {
      const unsigned long doubled = currentRetryMs_ * 2UL;
      currentRetryMs_ = doubled > config_.retryMaxMs ? config_.retryMaxMs : doubled;
    }
  }

  bool isConnected() const {
    return connected_;
  }

  unsigned long connectAttempts() const {
    return connectAttempts_;
  }

  unsigned long reconnectTransitions() const {
    return reconnectTransitions_;
  }

  unsigned long stateNotifications() const {
    return stateNotifications_;
  }

 private:
  void notifyState_(bool connected, unsigned long nowMs) {
    if (config_.emitStateChangesOnly && connected == lastObservedConnected_) {
      return;
    }

    lastObservedConnected_ = connected;
    ++stateNotifications_;

    if (stateWriter_ != NULL) {
      stateWriter_(connected, nowMs);
    }

    if (config_.stateTopicKey != 0) {
      kernel_->publishTypedFast(config_.stateTopicKey, Kernel::EventValue::fromBool(connected));
    }
  }

  void applyNetworkCapabilities_(bool connected) {
    if (!config_.manageCapabilities) {
      return;
    }

    if (connected) {
      kernel_->enableCapabilities(config_.capabilityMask);
    } else {
      kernel_->disableCapabilities(config_.capabilityMask);
    }
  }

  unsigned long computeRetryJitter_(unsigned long nowMs) const {
    if (config_.retryJitterMs == 0) {
      return 0;
    }

    return (nowMs + connectAttempts_ + reconnectTransitions_) % (config_.retryJitterMs + 1UL);
  }

  Kernel* kernel_;
  LinkProbe probe_;
  ConnectStep connectStep_;
  DisconnectStep disconnectStep_;
  StateWriter stateWriter_;
  Config config_;
  bool started_;
  bool hasPolled_;
  bool connected_;
  bool lastObservedConnected_;
  unsigned long lastPollAtMs_;
  unsigned long nextRetryAtMs_;
  unsigned long currentRetryMs_;
  unsigned long connectAttempts_;
  unsigned long reconnectTransitions_;
  unsigned long stateNotifications_;
  uint8_t consecutiveStablePolls_;
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
