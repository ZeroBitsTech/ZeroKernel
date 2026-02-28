#include <stdio.h>
#include <time.h>

#include "ZeroKernel.h"

namespace {

unsigned long g_stringRuns = 0;
unsigned long g_fastRuns = 0;
zerokernel::Kernel::TopicKey g_fastTopicKey = 0;

void stringBenchmarkTask() {
  ++g_stringRuns;
  ZeroKernel.publishDeferred("bench.metric", static_cast<long>(g_stringRuns));
}

void fastBenchmarkTask() {
  ++g_fastRuns;
  ZeroKernel.publishDeferredFast(g_fastTopicKey, static_cast<long>(g_fastRuns));
}

void sink(const char*, long) {}

double runBenchmark(bool fastPath,
                    unsigned long iterations,
                    unsigned long& outRuns,
                    unsigned long& outDelivered) {
  ZeroKernel = zerokernel::Kernel();
  outRuns = 0;

  if (fastPath) {
    g_fastTopicKey = zerokernel::Kernel::makeTopicKey("bench.metric");
    ZeroKernel.subscribeFast(g_fastTopicKey, sink);
    ZeroKernel.addTask("BenchTask", fastBenchmarkTask, 1, 0);
  } else {
    ZeroKernel.subscribe("bench.metric", sink);
    ZeroKernel.addTask("BenchTask", stringBenchmarkTask, 1, 0);
  }

  const clock_t beginClock = clock();

  for (unsigned long nowMs = 0; nowMs < iterations; ++nowMs) {
    ZeroKernel.tick(nowMs);
  }

  ZeroKernel.flushEvents();
  const clock_t endClock = clock();
  const zerokernel::Kernel::KernelStats stats = ZeroKernel.getStats();

  outRuns = fastPath ? g_fastRuns : g_stringRuns;
  outDelivered = stats.eventsDelivered;
  return static_cast<double>(endClock - beginClock) /
         static_cast<double>(CLOCKS_PER_SEC);
}

}  // namespace

int main() {
  const unsigned long iterations = 50000;
  unsigned long stringRuns = 0;
  unsigned long stringDelivered = 0;
  unsigned long fastRuns = 0;
  unsigned long fastDelivered = 0;

  const double stringSeconds =
      runBenchmark(false, iterations, stringRuns, stringDelivered);
  const double fastSeconds = runBenchmark(true, iterations, fastRuns, fastDelivered);

  printf("mode=string iterations=%lu runs=%lu events=%lu seconds=%.6f\n",
         iterations,
         stringRuns,
         stringDelivered,
         stringSeconds);
  printf("mode=fast iterations=%lu runs=%lu events=%lu seconds=%.6f\n",
         iterations,
         fastRuns,
         fastDelivered,
         fastSeconds);

  return 0;
}
