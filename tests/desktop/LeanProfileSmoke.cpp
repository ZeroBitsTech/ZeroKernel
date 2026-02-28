#include <stdio.h>

#include "ZeroKernel.h"

namespace {

int g_failures = 0;
int g_events = 0;
int g_dump_lines = 0;

void expectTrue(bool condition, const char* message) {
  if (!condition) {
    ++g_failures;
    printf("FAIL: %s\n", message);
  }
}

void onEvent(const char*, long value) {
  if (value == 11) {
    ++g_events;
  }
}

void onDump(const char*) {
  ++g_dump_lines;
}

void taskRun() {}

}  // namespace

int main() {
  zerokernel::Kernel kernel;

  expectTrue(kernel.addTask("Lean", taskRun, 50, 0), "add lean task");
  expectTrue(kernel.subscribe("lean.topic", onEvent), "subscribe lean topic");
  expectTrue(kernel.publish("lean.topic", 11), "publish lean topic");
  expectTrue(g_events == 1, "event delivered in lean profile");

  kernel.dumpStats(onDump);
  kernel.dumpTasks(onDump);
  kernel.dumpTrace(onDump);
  expectTrue(g_dump_lines == 0, "diagnostics compile to no-op");

  if (g_failures == 0) {
    printf("Lean profile smoke passed.\n");
    return 0;
  }

  return 1;
}
