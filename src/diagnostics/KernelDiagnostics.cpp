#include "../ZeroKernel.h"

#if ZEROKERNEL_ENABLE_DIAGNOSTICS

#include <stdio.h>

namespace zerokernel {

namespace {

void writeLine(Kernel::TextWriter writer, const char* line) {
  if (writer != NULL) {
    writer(line);
  }
}

}  // namespace

void Kernel::dumpStats(TextWriter writer) const {
#if !ZEROKERNEL_ENABLE_DEBUG_DUMP
  (void)writer;
  return;
#else
  if (writer == NULL) {
    return;
  }

  const KernelStats stats = getStats();
  const TimingReport timing = getTimingReport();
  char line[192];

  snprintf(line,
           sizeof(line),
           "kernel name=%s version=%s abi=%u state=%u uptime_ms=%lu loops=%lu idle=%lu runs=%lu",
           identity().name,
           runtimeVersion(),
           static_cast<unsigned int>(abiVersion()),
           static_cast<unsigned int>(state()),
           static_cast<unsigned long>(stats.uptimeMs),
           static_cast<unsigned long>(stats.schedulerLoops),
           static_cast<unsigned long>(stats.schedulerIdleLoops),
           static_cast<unsigned long>(stats.taskExecutions));
  writeLine(writer, line);

  snprintf(line,
           sizeof(line),
           "kernel failures=%lu recoveries=%lu deadline_miss=%lu",
           static_cast<unsigned long>(stats.taskFailures),
           static_cast<unsigned long>(stats.taskRecoveries),
           static_cast<unsigned long>(stats.deadlineMisses));
  writeLine(writer, line);

  snprintf(line,
           sizeof(line),
           "kernel events=%lu delivered=%lu drops=%lu",
           static_cast<unsigned long>(stats.eventsPublished),
           static_cast<unsigned long>(stats.eventsDelivered),
           static_cast<unsigned long>(stats.queuedEventsDropped));
  writeLine(writer, line);

  snprintf(line,
           sizeof(line),
           "kernel commands=%lu delivered=%lu drops=%lu",
           static_cast<unsigned long>(stats.commandsQueued),
           static_cast<unsigned long>(stats.commandsDelivered),
           static_cast<unsigned long>(stats.queuedCommandsDropped));
  writeLine(writer, line);

  snprintf(line,
           sizeof(line),
           "kernel work=%lu delivered=%lu drops=%lu",
           static_cast<unsigned long>(stats.workQueued),
           static_cast<unsigned long>(stats.workDelivered),
           static_cast<unsigned long>(stats.queuedWorkDropped));
  writeLine(writer, line);

  snprintf(line,
           sizeof(line),
           "kernel timing tick_max_ms=%lu tick_avg_ms=%lu task_max_ms=%lu task_avg_ms=%lu lag_max_ms=%lu lag_avg_ms=%lu tick_max_cycles=%lu task_max_cycles=%lu",
           static_cast<unsigned long>(timing.worstTickDurationMs),
           static_cast<unsigned long>(timing.averageTickDurationMs),
           static_cast<unsigned long>(timing.worstTaskDurationMs),
           static_cast<unsigned long>(timing.averageTaskDurationMs),
           static_cast<unsigned long>(timing.worstLagMs),
           static_cast<unsigned long>(timing.averageLagMs),
           static_cast<unsigned long>(timing.worstTickCycles),
           static_cast<unsigned long>(timing.worstTaskCycles));
  writeLine(writer, line);

  const PanicInfo panicInfo = getLastPanic();
  if (panicInfo.reason != kPanicNone) {
    snprintf(line,
             sizeof(line),
             "kernel panic reason=%u mode=%u task=%s observed=%lu budget=%lu",
             static_cast<unsigned int>(panicInfo.reason),
             static_cast<unsigned int>(panicInfo.mode),
             panicInfo.taskName,
             static_cast<unsigned long>(panicInfo.observedValue),
             static_cast<unsigned long>(panicInfo.budgetValue));
    writeLine(writer, line);
  }
#endif
}

void Kernel::dumpTasks(TextWriter writer) const {
#if !ZEROKERNEL_ENABLE_DEBUG_DUMP
  (void)writer;
  return;
#else
  if (writer == NULL) {
    return;
  }

  TaskStats snapshots[kTaskStorage];
  const uint8_t count = snapshotTasks(snapshots, kTaskStorage);
  char line[160];

  for (uint8_t i = 0; i < count; ++i) {
    const TaskStats& task = snapshots[i];
    snprintf(line,
             sizeof(line),
             "task name=%s state=%u prio=%u runs=%lu lag_ms=%lu lag_max_ms=%lu "
             "deadline_miss=%lu failures=%u flags=%u",
             task.name,
             static_cast<unsigned int>(task.state),
             static_cast<unsigned int>(task.priority),
             static_cast<unsigned long>(task.runCount),
             static_cast<unsigned long>(task.lastLagMs),
             static_cast<unsigned long>(task.maxLagMs),
             static_cast<unsigned long>(task.deadlineMissCount),
             static_cast<unsigned int>(task.failureCount),
             static_cast<unsigned int>(task.contractFlags));
    writeLine(writer, line);
  }
#endif
}

void Kernel::dumpTrace(TextWriter writer) const {
#if !ZEROKERNEL_ENABLE_DEBUG_DUMP
  (void)writer;
  return;
#else
  if (writer == NULL) {
    return;
  }

  TraceEntry entries[kTraceStorage];
  const uint8_t count = snapshotTrace(entries, kMaxTraceEntries);
  char line[160];

  for (uint8_t i = 0; i < count; ++i) {
    snprintf(line,
             sizeof(line),
             "trace ts=%lu type=%u label=%s value=%lu",
             static_cast<unsigned long>(entries[i].timestampMs),
             static_cast<unsigned int>(entries[i].type),
             entries[i].label,
             static_cast<unsigned long>(entries[i].value));
    writeLine(writer, line);
  }
#endif
}

}  // namespace zerokernel

#else

namespace zerokernel {

void Kernel::dumpStats(TextWriter writer) const {
  (void)writer;
}

void Kernel::dumpTasks(TextWriter writer) const {
  (void)writer;
}

void Kernel::dumpTrace(TextWriter writer) const {
  (void)writer;
}

}  // namespace zerokernel

#endif
