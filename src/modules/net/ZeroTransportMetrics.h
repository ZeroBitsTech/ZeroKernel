#ifndef ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H
#define ZEROKERNEL_MODULES_NET_ZEROTRANSPORTMETRICS_H

namespace zerokernel {
namespace modules {
namespace net {

// Optional transport metrics helper. Header-only and only linked when included.
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
    snapshot_.connectAttempts = 0;
    snapshot_.connectSuccesses = 0;
    snapshot_.connectFailures = 0;
    snapshot_.sendAttempts = 0;
    snapshot_.sendSuccesses = 0;
    snapshot_.sendFailures = 0;
    snapshot_.loopCalls = 0;
    snapshot_.queueDrops = 0;
    snapshot_.backoffSchedules = 0;
    snapshot_.consecutiveFailures = 0;
    snapshot_.lastConnectLatencyMs = 0;
    snapshot_.worstConnectLatencyMs = 0;
    snapshot_.lastSendLatencyMs = 0;
    snapshot_.worstSendLatencyMs = 0;
    snapshot_.lastQueueDwellMs = 0;
    snapshot_.worstQueueDwellMs = 0;
    snapshot_.maxQueueDepth = 0;
  }

  void recordConnectAttempt() {
    ++snapshot_.connectAttempts;
  }

  void recordConnectResult(bool success, unsigned long latencyMs) {
    snapshot_.lastConnectLatencyMs = latencyMs;
    if (latencyMs > snapshot_.worstConnectLatencyMs) {
      snapshot_.worstConnectLatencyMs = latencyMs;
    }

    if (success) {
      ++snapshot_.connectSuccesses;
      snapshot_.consecutiveFailures = 0;
      return;
    }

    ++snapshot_.connectFailures;
    ++snapshot_.consecutiveFailures;
  }

  void recordSendQueued(unsigned long queueDepth) {
    if (queueDepth > snapshot_.maxQueueDepth) {
      snapshot_.maxQueueDepth = queueDepth;
    }
  }

  void recordSendAttempt() {
    ++snapshot_.sendAttempts;
  }

  void recordSendResult(bool success,
                        unsigned long latencyMs,
                        unsigned long queueDwellMs) {
    snapshot_.lastSendLatencyMs = latencyMs;
    snapshot_.lastQueueDwellMs = queueDwellMs;

    if (latencyMs > snapshot_.worstSendLatencyMs) {
      snapshot_.worstSendLatencyMs = latencyMs;
    }

    if (queueDwellMs > snapshot_.worstQueueDwellMs) {
      snapshot_.worstQueueDwellMs = queueDwellMs;
    }

    if (success) {
      ++snapshot_.sendSuccesses;
      snapshot_.consecutiveFailures = 0;
      return;
    }

    ++snapshot_.sendFailures;
    ++snapshot_.consecutiveFailures;
  }

  void recordLoopCall() {
    ++snapshot_.loopCalls;
  }

  void recordQueueDrop() {
    ++snapshot_.queueDrops;
  }

  void recordBackoffSchedule() {
    ++snapshot_.backoffSchedules;
  }

  const Snapshot& snapshot() const {
    return snapshot_;
  }

 private:
  Snapshot snapshot_;
};

}  // namespace net
}  // namespace modules
}  // namespace zerokernel

#endif
