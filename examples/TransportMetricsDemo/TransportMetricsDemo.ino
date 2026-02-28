#include <ZeroKernel.h>
#include <modules/net/ZeroTransportMetrics.h>

using zerokernel::modules::net::ZeroTransportMetrics;

namespace {

ZeroTransportMetrics g_metrics;

void updateMetricsTask() {
  static bool toggle = false;
  toggle = !toggle;

  g_metrics.recordConnectAttempt();
  g_metrics.recordConnectResult(toggle, toggle ? 2 : 5);
  g_metrics.recordSendQueued(2);
  g_metrics.recordSendAttempt();
  g_metrics.recordSendResult(toggle, toggle ? 1 : 4, toggle ? 3 : 8);
  if (!toggle) {
    g_metrics.recordQueueDrop();
    g_metrics.recordBackoffSchedule();
  }
  g_metrics.recordLoopCall();
}

void reportTask() {
  const ZeroTransportMetrics::Snapshot snapshot = g_metrics.snapshot();

  Serial.print("connect_ok=");
  Serial.print(snapshot.connectSuccesses);
  Serial.print(" connect_fail=");
  Serial.print(snapshot.connectFailures);
  Serial.print(" send_ok=");
  Serial.print(snapshot.sendSuccesses);
  Serial.print(" send_fail=");
  Serial.println(snapshot.sendFailures);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin();
  ZeroKernel.addTask("MetricsTick", updateMetricsTask, 500, 0);
  ZeroKernel.addTask("MetricsReport", reportTask, 1000, 0);
}

void loop() {
  ZeroKernel.tick();
}
