#include <ZeroKernel.h>
#include <modules/net/ZeroHttpPump.h>

using zerokernel::Kernel;
using zerokernel::modules::net::ZeroHttpPump;

namespace {

ZeroHttpPump g_httpPump;
bool g_connectPending = true;
unsigned long g_requestId = 0;
const Kernel::TopicKey kCompletionTopic = Kernel::makeTopicKey("http.done");

ZeroHttpPump::StepResult connectStep(const ZeroHttpPump::Request&, void*) {
  if (g_connectPending) {
    g_connectPending = false;
    return ZeroHttpPump::kStepPending;
  }

  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult writeStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult readStep(const ZeroHttpPump::Request&, void*) {
  return ZeroHttpPump::kStepComplete;
}

ZeroHttpPump::StepResult closeStep(const ZeroHttpPump::Request&, void*) {
  g_connectPending = true;
  return ZeroHttpPump::kStepComplete;
}

void onCompletion(const char*, const Kernel::EventValue& value) {
  if (value.type != Kernel::kEventBool) {
    return;
  }

  Serial.print("http.done => ");
  Serial.println(value.boolValue ? "ok" : "fail");
}

void queueRequestTask() {
  static char payload[48];
  const int written = snprintf(payload, sizeof(payload), "{\"seq\":%lu}", g_requestId++);
  if (written <= 0) {
    return;
  }

  ZeroHttpPump::Request request;
  request.path = "/api/data";
  request.contentType = "application/json";
  request.body = payload;
  request.bodyLength = static_cast<uint16_t>(written);
  request.completionTopicKey = kCompletionTopic;
  g_httpPump.enqueue(request);
}

void reportTask() {
  const zerokernel::modules::net::ZeroTransportMetrics::Snapshot snapshot =
      g_httpPump.metrics().snapshot();

  Serial.print("http queue=");
  Serial.print(g_httpPump.queuedCount());
  Serial.print(" sent_ok=");
  Serial.print(snapshot.sendSuccesses);
  Serial.print(" sent_fail=");
  Serial.print(snapshot.sendFailures);
  Serial.print(" connect_attempts=");
  Serial.println(snapshot.connectAttempts);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(50);

  ZeroKernel.begin();
  ZeroKernel.subscribeTypedFast(kCompletionTopic, onCompletion);

  ZeroHttpPump::Config config;
  config.retryBaseMs = 100;
  config.retryMaxMs = 500;
  config.maxRetries = 1;

  g_httpPump.begin(ZeroKernel, connectStep, writeStep, readStep, closeStep, config);

  ZeroKernel.addTask("HttpQueue", queueRequestTask, 1500, 0);
  ZeroKernel.addTask("HttpReport", reportTask, 1000, 0);
}

void loop() {
  ZeroKernel.tick();
  g_httpPump.tick();
}
