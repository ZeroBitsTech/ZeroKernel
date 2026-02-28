#ifndef ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H
#define ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H

#include <stdint.h>

namespace zerokernel {
namespace modules {
namespace net {

// Optional transport metrics helper. Header-only and only linked when included.
// BETA: validated on desktop and ESP32 smoke tests, still under active tuning.
class ZeroTransportMetrics {
 public:
  struct Snapshot {
    unsigned long connectAttempts;
    unsigned long connectSuccesses;
    unsigned long connectFailures;
    unsigned long sendAttempts;
    unsigned long sendSuccesses;
    unsigned long sendFailures;
    unsigned long loopCalls;
    unsigned long queueDrops;
    unsigned long backoffSchedules;
    unsigned long consecutiveFailures;
    unsigned long lastConnectLatencyMs;
    unsigned long worstConnectLatencyMs;
    unsigned long lastSendLatencyMs;
    unsigned long worstSendLatencyMs;
    unsigned long lastQueueDwellMs;
    unsigned long worstQueueDwellMs;
    unsigned long maxQueueDepth;
  };

  ZeroTransportMetrics() {
    reset();
  }

  void reset() {
    connectAttempts_ = 0;
    connectSuccesses_ = 0;
    connectFailures_ = 0;
    sendAttempts_ = 0;
    sendSuccesses_ = 0;
    sendFailures_ = 0;
    loopCalls_ = 0;
    queueDrops_ = 0;
    backoffSchedules_ = 0;
    consecutiveFailures_ = 0;
    maxQueueDepth_ = 0;

#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
    lastConnectLatencyMs_ = 0;
    worstConnectLatencyMs_ = 0;
    lastSendLatencyMs_ = 0;
    worstSendLatencyMs_ = 0;
    lastQueueDwellMs_ = 0;
    worstQueueDwellMs_ = 0;
#endif
  }

  void recordConnectAttempt() {
    increment16_(connectAttempts_);
  }

  void recordConnectResult(bool success, unsigned long latencyMs) {
#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
    lastConnectLatencyMs_ = latencyMs;
    if (latencyMs > worstConnectLatencyMs_) {
      worstConnectLatencyMs_ = latencyMs;
    }
#endif

    if (success) {
      increment16_(connectSuccesses_);
      consecutiveFailures_ = 0;
      return;
    }

    increment16_(connectFailures_);
    increment16_(consecutiveFailures_);
  }

  void recordSendQueued(unsigned long queueDepth) {
    assignQueueDepth_(maxQueueDepth_, queueDepth);
  }

  void recordSendAttempt() {
    increment16_(sendAttempts_);
  }

  void recordSendResult(bool success,
                        unsigned long latencyMs,
                        unsigned long queueDwellMs) {
#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
    lastSendLatencyMs_ = latencyMs;
    lastQueueDwellMs_ = queueDwellMs;

    if (latencyMs > worstSendLatencyMs_) {
      worstSendLatencyMs_ = latencyMs;
    }

    if (queueDwellMs > worstQueueDwellMs_) {
      worstQueueDwellMs_ = queueDwellMs;
    }
#endif

    if (success) {
      increment16_(sendSuccesses_);
      consecutiveFailures_ = 0;
      return;
    }

    increment16_(sendFailures_);
    increment16_(consecutiveFailures_);
  }

  void recordLoopCall() {
    increment16_(loopCalls_);
  }

  void recordQueueDrop() {
    increment16_(queueDrops_);
  }

  void recordBackoffSchedule() {
    increment16_(backoffSchedules_);
  }

  Snapshot snapshot() const {
    Snapshot snapshot;
    snapshot.connectAttempts = connectAttempts_;
    snapshot.connectSuccesses = connectSuccesses_;
    snapshot.connectFailures = connectFailures_;
    snapshot.sendAttempts = sendAttempts_;
    snapshot.sendSuccesses = sendSuccesses_;
    snapshot.sendFailures = sendFailures_;
    snapshot.loopCalls = loopCalls_;
    snapshot.queueDrops = queueDrops_;
    snapshot.backoffSchedules = backoffSchedules_;
    snapshot.consecutiveFailures = consecutiveFailures_;
    snapshot.maxQueueDepth = maxQueueDepth_;

#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
    snapshot.lastConnectLatencyMs = lastConnectLatencyMs_;
    snapshot.worstConnectLatencyMs = worstConnectLatencyMs_;
    snapshot.lastSendLatencyMs = lastSendLatencyMs_;
    snapshot.worstSendLatencyMs = worstSendLatencyMs_;
    snapshot.lastQueueDwellMs = lastQueueDwellMs_;
    snapshot.worstQueueDwellMs = worstQueueDwellMs_;
#else
    snapshot.lastConnectLatencyMs = 0;
    snapshot.worstConnectLatencyMs = 0;
    snapshot.lastSendLatencyMs = 0;
    snapshot.worstSendLatencyMs = 0;
    snapshot.lastQueueDwellMs = 0;
    snapshot.worstQueueDwellMs = 0;
#endif

    return snapshot;
  }

 private:
  static void increment16_(uint16_t& value) {
    if (value != 0xFFFFu) {
      ++value;
    }
  }

  static void assignQueueDepth_(uint8_t& target, unsigned long depth) {
    const uint8_t normalized = depth > 0xFFu ? 0xFFu : static_cast<uint8_t>(depth);
    if (normalized > target) {
      target = normalized;
    }
  }

  uint16_t connectAttempts_;
  uint16_t connectSuccesses_;
  uint16_t connectFailures_;
  uint16_t sendAttempts_;
  uint16_t sendSuccesses_;
  uint16_t sendFailures_;
  uint16_t loopCalls_;
  uint16_t queueDrops_;
  uint16_t backoffSchedules_;
  uint16_t consecutiveFailures_;
  uint8_t maxQueueDepth_;

#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
  unsigned long lastConnectLatencyMs_;
  unsigned long worstConnectLatencyMs_;
  unsigned long lastSendLatencyMs_;
  unsigned long worstSendLatencyMs_;
  unsigned long lastQueueDwellMs_;
  unsigned long worstQueueDwellMs_;
#endif
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
