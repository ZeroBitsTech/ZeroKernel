#ifndef ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H
#define ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H

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
    ++connectAttempts_;
  }

  void recordConnectResult(bool success, unsigned long latencyMs) {
#if ZEROKERNEL_ENABLE_NET_EXTENDED_METRICS
    lastConnectLatencyMs_ = latencyMs;
    if (latencyMs > worstConnectLatencyMs_) {
      worstConnectLatencyMs_ = latencyMs;
    }
#endif

    if (success) {
      ++connectSuccesses_;
      consecutiveFailures_ = 0;
      return;
    }

    ++connectFailures_;
    ++consecutiveFailures_;
  }

  void recordSendQueued(unsigned long queueDepth) {
    if (queueDepth > maxQueueDepth_) {
      maxQueueDepth_ = queueDepth;
    }
  }

  void recordSendAttempt() {
    ++sendAttempts_;
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
      ++sendSuccesses_;
      consecutiveFailures_ = 0;
      return;
    }

    ++sendFailures_;
    ++consecutiveFailures_;
  }

  void recordLoopCall() {
    ++loopCalls_;
  }

  void recordQueueDrop() {
    ++queueDrops_;
  }

  void recordBackoffSchedule() {
    ++backoffSchedules_;
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
  unsigned long connectAttempts_;
  unsigned long connectSuccesses_;
  unsigned long connectFailures_;
  unsigned long sendAttempts_;
  unsigned long sendSuccesses_;
  unsigned long sendFailures_;
  unsigned long loopCalls_;
  unsigned long queueDrops_;
  unsigned long backoffSchedules_;
  unsigned long consecutiveFailures_;
  unsigned long maxQueueDepth_;

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
