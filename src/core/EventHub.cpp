#include "../ZeroKernel.h"
#include "../internal/KernelArch.h"
#include "../internal/KernelString.h"

namespace zerokernel {

bool Kernel::publish(const char* topic, long value) {
  return publishTyped(topic, EventValue::fromLong(value));
}

bool Kernel::publishTyped(const char* topic, const EventValue& value) {
#if ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY
  if (publishTypedFast(topicKey_(topic), value)) {
    return true;
  }

  return false;
#else
  if (dispatchEvent_(topic, value)) {
    ++kernelStats_.eventsPublished;
    return true;
  }

  return false;
#endif
}

bool Kernel::publishFast(TopicKey topicKey, long value) {
  return publishTypedFast(topicKey, EventValue::fromLong(value));
}

bool Kernel::publishTypedFast(TopicKey topicKey, const EventValue& value) {
  if (dispatchEventByKey_(topicKey, value)) {
    ++kernelStats_.eventsPublished;
    return true;
  }

  return false;
}

bool Kernel::publishDeferred(const char* topic, long value) {
  return publishDeferredTyped(topic, EventValue::fromLong(value));
}

bool Kernel::publishDeferredTyped(const char* topic, const EventValue& value) {
#if ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY
  if (enqueueEventByKey_(topicKey_(topic), value)) {
    ++kernelStats_.eventsPublished;
    return true;
  }

  ++kernelStats_.queuedEventsDropped;
  emitSignal_(kSignalEventDrop, "event.queue", kernelStats_.queuedEventsDropped);
  return false;
#else
  if (enqueueEvent_(topic, value)) {
    ++kernelStats_.eventsPublished;
    return true;
  }

  ++kernelStats_.queuedEventsDropped;
  emitSignal_(kSignalEventDrop, topic, kernelStats_.queuedEventsDropped);
  return false;
#endif
}

bool Kernel::publishDeferredFast(TopicKey topicKey, long value) {
  return publishDeferredTypedFast(topicKey, EventValue::fromLong(value));
}

bool Kernel::publishDeferredTypedFast(TopicKey topicKey, const EventValue& value) {
  if (enqueueEventByKey_(topicKey, value)) {
    ++kernelStats_.eventsPublished;
    return true;
  }

  ++kernelStats_.queuedEventsDropped;
  emitSignal_(kSignalEventDrop, "event.queue", kernelStats_.queuedEventsDropped);
  return false;
}

bool Kernel::enqueueCommand(const char* command, long value) {
  return enqueueCommandTyped(command, EventValue::fromLong(value));
}

bool Kernel::enqueueCommandTyped(const char* command, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)command;
  (void)value;
  return false;
#else
#if ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY
  if (enqueueCommandByKey_(topicKey_(command), value)) {
    return true;
  }

  ++kernelStats_.queuedCommandsDropped;
  emitSignal_(kSignalCommandDrop, "command.queue", kernelStats_.queuedCommandsDropped);
  return false;
#else
  if (enqueueCommand_(command, value)) {
    return true;
  }

  ++kernelStats_.queuedCommandsDropped;
  emitSignal_(kSignalCommandDrop, command, kernelStats_.queuedCommandsDropped);
  return false;
#endif
#endif
}

bool Kernel::enqueueCommandFast(TopicKey commandKey, long value) {
  return enqueueCommandTypedFast(commandKey, EventValue::fromLong(value));
}

bool Kernel::enqueueCommandTypedFast(TopicKey commandKey, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)commandKey;
  (void)value;
  return false;
#else
  if (enqueueCommandByKey_(commandKey, value)) {
    return true;
  }

  ++kernelStats_.queuedCommandsDropped;
  emitSignal_(kSignalCommandDrop, "command.queue", kernelStats_.queuedCommandsDropped);
  return false;
#endif
}

bool Kernel::scheduleWork(WorkHandler handler) {
  return scheduleWorkTyped(handler, EventValue::fromLong(0));
}

bool Kernel::scheduleWorkTyped(WorkHandler handler, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  (void)handler;
  (void)value;
  return false;
#else
  if (enqueueWork_(handler, value)) {
    return true;
  }

  ++kernelStats_.queuedWorkDropped;
  emitSignal_(kSignalWorkDrop, "work.queue", kernelStats_.queuedWorkDropped);
  return false;
#endif
}

bool Kernel::dispatchEvent_(const char* topic, const EventValue& value) {
  if (topic == NULL) {
    return false;
  }

#if ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY
  return dispatchEventByKey_(topicKey_(topic), value);
#else

  const TopicKey topicKey = topicKey_(topic);
  bool delivered = false;

  if (value.type == kEventLong) {
    for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
      SubscriptionSlot& slot = subscribers_[i];
      if (!slot.inUse || slot.handler == NULL) {
        continue;
      }

      if (slot.topicKey != topicKey) {
        continue;
      }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      if (slot.topic != NULL &&
          !internal::labelsEqual(slot.topic, topic, kTopicNameLength)) {
        continue;
      }
#endif

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      slot.handler(labelOrEmpty_(slot.topic), value.longValue);
#else
      slot.handler("", value.longValue);
#endif
      delivered = true;
      ++kernelStats_.eventsDelivered;
    }
  }

#if ZEROKERNEL_ENABLE_TYPED_EVENTS
  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    TypedSubscriptionSlot& slot = typedSubscribers_[i];
    if (!slot.inUse || slot.handler == NULL) {
      continue;
    }

    if (slot.topicKey != topicKey) {
      continue;
    }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    if (slot.topic != NULL &&
        !internal::labelsEqual(slot.topic, topic, kTopicNameLength)) {
      continue;
    }
#endif

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    slot.handler(labelOrEmpty_(slot.topic), value);
#else
    slot.handler("", value);
#endif
    delivered = true;
    ++kernelStats_.eventsDelivered;
  }
#endif

  return delivered;
#endif
}

bool Kernel::dispatchEventByKey_(TopicKey topicKey, const EventValue& value) {
  if (topicKey == 0U) {
    return false;
  }

  bool delivered = false;

  if (value.type == kEventLong) {
    for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
      SubscriptionSlot& slot = subscribers_[i];
      if (!slot.inUse || slot.handler == NULL || slot.topicKey != topicKey) {
        continue;
      }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      slot.handler(labelOrEmpty_(slot.topic), value.longValue);
#else
      slot.handler("", value.longValue);
#endif
      delivered = true;
      ++kernelStats_.eventsDelivered;
    }
  }

#if ZEROKERNEL_ENABLE_TYPED_EVENTS
  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    TypedSubscriptionSlot& slot = typedSubscribers_[i];
    if (!slot.inUse || slot.handler == NULL || slot.topicKey != topicKey) {
      continue;
    }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    slot.handler(labelOrEmpty_(slot.topic), value);
#else
    slot.handler("", value);
#endif
    delivered = true;
    ++kernelStats_.eventsDelivered;
  }
#endif

  return delivered;
}

bool Kernel::dispatchCommand_(const char* command, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)command;
  (void)value;
  return false;
#else
  if (command == NULL) {
    return false;
  }

#if ZEROKERNEL_ENABLE_TOPIC_KEY_ONLY
  return dispatchCommandByKey_(topicKey_(command), value);
#else

  const TopicKey commandKey = topicKey_(command);
  bool delivered = false;

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    CommandHandlerSlot& slot = commandHandlers_[i];
    if (!slot.inUse || slot.handler == NULL) {
      continue;
    }

    if (slot.topicKey != commandKey) {
      continue;
    }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    if (slot.topic != NULL &&
        !internal::labelsEqual(slot.topic, command, kTopicNameLength)) {
      continue;
    }
#endif

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    slot.handler(labelOrEmpty_(slot.topic), value);
#else
    slot.handler("", value);
#endif
    delivered = true;
    ++kernelStats_.commandsDelivered;
  }

  return delivered;
#endif
#endif
}

bool Kernel::dispatchCommandByKey_(TopicKey commandKey, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)commandKey;
  (void)value;
  return false;
#else
  if (commandKey == 0U) {
    return false;
  }

  bool delivered = false;

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    CommandHandlerSlot& slot = commandHandlers_[i];
    if (!slot.inUse || slot.handler == NULL || slot.topicKey != commandKey) {
      continue;
    }

#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
    slot.handler(labelOrEmpty_(slot.topic), value);
#else
    slot.handler("", value);
#endif
    delivered = true;
    ++kernelStats_.commandsDelivered;
  }

  return delivered;
#endif
}

bool Kernel::subscribe(const char* topic, EventHandler handler) {
  if (topic == NULL || handler == NULL) {
    return false;
  }

  const TopicKey topicKey = topicKey_(topic);
  return subscribeFast(
      topicKey, handler, ZEROKERNEL_ENABLE_LEGACY_LABEL_API ? topic : NULL);
}

bool Kernel::subscribeFast(TopicKey topicKey, EventHandler handler, const char* topicLabel) {
  if (topicKey == 0U || handler == NULL) {
    return false;
  }

  for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
    if (!subscribers_[i].inUse) {
      continue;
    }

    if (subscribers_[i].handler == handler && subscribers_[i].topicKey == topicKey) {
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      const char* existingLabel = subscribers_[i].topic;
      if (existingLabel == NULL || topicLabel == NULL ||
          internal::labelsEqual(existingLabel, topicLabel, kTopicNameLength)) {
        return false;
      }
#else
      return false;
#endif
    }
  }

  const int freeIndex = findFreeSubscriberIndex_();
  if (freeIndex < 0) {
    return false;
  }

  SubscriptionSlot& slot = subscribers_[freeIndex];
  slot = SubscriptionSlot();
  slot.inUse = true;
  slot.topicKey = topicKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  slot.topic = topicLabel;
#else
  (void)topicLabel;
#endif
  slot.handler = handler;
  return true;
}

bool Kernel::subscribeTyped(const char* topic, TypedEventHandler handler) {
#if !ZEROKERNEL_ENABLE_TYPED_EVENTS
  (void)topic;
  (void)handler;
  return false;
#else
  if (topic == NULL || handler == NULL) {
    return false;
  }

  const TopicKey topicKey = topicKey_(topic);
  return subscribeTypedFast(
      topicKey, handler, ZEROKERNEL_ENABLE_LEGACY_LABEL_API ? topic : NULL);
}

bool Kernel::subscribeTypedFast(TopicKey topicKey,
                                TypedEventHandler handler,
                                const char* topicLabel) {
  if (topicKey == 0U || handler == NULL) {
    return false;
  }

  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    if (!typedSubscribers_[i].inUse) {
      continue;
    }

    if (typedSubscribers_[i].handler == handler && typedSubscribers_[i].topicKey == topicKey) {
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      const char* existingLabel = typedSubscribers_[i].topic;
      if (existingLabel == NULL || topicLabel == NULL ||
          internal::labelsEqual(existingLabel, topicLabel, kTopicNameLength)) {
        return false;
      }
#else
      return false;
#endif
    }
  }

  const int freeIndex = findFreeTypedSubscriberIndex_();
  if (freeIndex < 0) {
    return false;
  }

  TypedSubscriptionSlot& slot = typedSubscribers_[freeIndex];
  slot = TypedSubscriptionSlot();
  slot.inUse = true;
  slot.topicKey = topicKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  slot.topic = topicLabel;
#else
  (void)topicLabel;
#endif
  slot.handler = handler;
  return true;
#endif
}

bool Kernel::registerCommand(const char* command, CommandHandler handler) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)command;
  (void)handler;
  return false;
#else
  if (command == NULL || handler == NULL) {
    return false;
  }

  const TopicKey commandKey = topicKey_(command);
  return registerCommandFast(
      commandKey, handler, ZEROKERNEL_ENABLE_LEGACY_LABEL_API ? command : NULL);
#endif
}

bool Kernel::registerCommandFast(TopicKey commandKey,
                                 CommandHandler handler,
                                 const char* commandLabel) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)commandKey;
  (void)handler;
  (void)commandLabel;
  return false;
#else
  if (commandKey == 0U || handler == NULL) {
    return false;
  }

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    if (!commandHandlers_[i].inUse) {
      continue;
    }

    if (commandHandlers_[i].handler == handler && commandHandlers_[i].topicKey == commandKey) {
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
      const char* existingLabel = commandHandlers_[i].topic;
      if (existingLabel == NULL || commandLabel == NULL ||
          internal::labelsEqual(existingLabel, commandLabel, kTopicNameLength)) {
        return false;
      }
#else
      return false;
#endif
    }
  }

  const int freeIndex = findFreeCommandHandlerIndex_();
  if (freeIndex < 0) {
    return false;
  }

  CommandHandlerSlot& slot = commandHandlers_[freeIndex];
  slot = CommandHandlerSlot();
  slot.inUse = true;
  slot.topicKey = commandKey;
#if ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  slot.topic = commandLabel;
#else
  (void)commandLabel;
#endif
  slot.handler = handler;
  return true;
#endif
}

bool Kernel::unsubscribe(const char* topic, EventHandler handler) {
  if (topic == NULL) {
    return false;
  }

  const TopicKey topicKey = topicKey_(topic);
#if !ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  return unsubscribeFast(topicKey, handler);
#else
  bool removed = false;

  for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
    SubscriptionSlot& slot = subscribers_[i];
    if (!slot.inUse) {
      continue;
    }

    if (slot.topicKey != topicKey) {
      continue;
    }

    if (slot.topic != NULL &&
        !internal::labelsEqual(slot.topic, topic, kTopicNameLength)) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = SubscriptionSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
#endif
}

bool Kernel::unsubscribeFast(TopicKey topicKey, EventHandler handler) {
  if (topicKey == 0U) {
    return false;
  }

  bool removed = false;

  for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
    SubscriptionSlot& slot = subscribers_[i];
    if (!slot.inUse || slot.topicKey != topicKey) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = SubscriptionSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
}

bool Kernel::unsubscribeTyped(const char* topic, TypedEventHandler handler) {
#if !ZEROKERNEL_ENABLE_TYPED_EVENTS
  (void)topic;
  (void)handler;
  return false;
#else
  if (topic == NULL) {
    return false;
  }

  const TopicKey topicKey = topicKey_(topic);
#if !ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  return unsubscribeTypedFast(topicKey, handler);
#else
  bool removed = false;

  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    TypedSubscriptionSlot& slot = typedSubscribers_[i];
    if (!slot.inUse) {
      continue;
    }

    if (slot.topicKey != topicKey) {
      continue;
    }

    if (slot.topic != NULL &&
        !internal::labelsEqual(slot.topic, topic, kTopicNameLength)) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = TypedSubscriptionSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
#endif
#endif
}

bool Kernel::unsubscribeTypedFast(TopicKey topicKey, TypedEventHandler handler) {
#if !ZEROKERNEL_ENABLE_TYPED_EVENTS
  (void)topicKey;
  (void)handler;
  return false;
#else
  if (topicKey == 0U) {
    return false;
  }

  bool removed = false;

  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    TypedSubscriptionSlot& slot = typedSubscribers_[i];
    if (!slot.inUse || slot.topicKey != topicKey) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = TypedSubscriptionSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
#endif
}

bool Kernel::unregisterCommand(const char* command, CommandHandler handler) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)command;
  (void)handler;
  return false;
#else
  if (command == NULL) {
    return false;
  }

  const TopicKey commandKey = topicKey_(command);
#if !ZEROKERNEL_ENABLE_LEGACY_LABEL_API
  return unregisterCommandFast(commandKey, handler);
#else
  bool removed = false;

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    CommandHandlerSlot& slot = commandHandlers_[i];
    if (!slot.inUse) {
      continue;
    }

    if (slot.topicKey != commandKey) {
      continue;
    }

    if (slot.topic != NULL &&
        !internal::labelsEqual(slot.topic, command, kTopicNameLength)) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = CommandHandlerSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
#endif
#endif
}

bool Kernel::unregisterCommandFast(TopicKey commandKey, CommandHandler handler) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)commandKey;
  (void)handler;
  return false;
#else
  if (commandKey == 0U) {
    return false;
  }

  bool removed = false;

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    CommandHandlerSlot& slot = commandHandlers_[i];
    if (!slot.inUse || slot.topicKey != commandKey) {
      continue;
    }

    if (handler != NULL && slot.handler != handler) {
      continue;
    }

    slot = CommandHandlerSlot();
    removed = true;

    if (handler != NULL) {
      break;
    }
  }

  return removed;
#endif
}

void Kernel::flushEvents() {
  drainEventQueue_();
}

void Kernel::flushCommands() {
  drainCommandQueue_();
}

void Kernel::flushWork() {
  drainWorkQueue_();
}

void Kernel::setFlags(EventFlags mask) {
  internal::atomicOrU32(&eventFlags_, mask);
}

void Kernel::clearFlags(EventFlags mask) {
  internal::atomicAndU32(&eventFlags_, ~mask);
}

bool Kernel::hasFlags(EventFlags mask, bool requireAll) const {
  const EventFlags currentFlags = internal::atomicLoadU32(&eventFlags_);

  if (requireAll) {
    return (currentFlags & mask) == mask;
  }

  return (currentFlags & mask) != 0U;
}

Kernel::EventFlags Kernel::takeFlags(EventFlags mask) {
  const EventFlags captured = internal::atomicLoadU32(&eventFlags_) & mask;
  internal::atomicAndU32(&eventFlags_, ~mask);
  return captured;
}

uint8_t Kernel::subscriptionCount() const {
  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
    if (subscribers_[i].inUse) {
      ++count;
    }
  }

  return count;
}

uint8_t Kernel::typedSubscriptionCount() const {
#if !ZEROKERNEL_ENABLE_TYPED_EVENTS
  return 0;
#else
  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    if (typedSubscribers_[i].inUse) {
      ++count;
    }
  }

  return count;
#endif
}

uint8_t Kernel::commandHandlerCount() const {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  return 0;
#else
  uint8_t count = 0;

  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    if (commandHandlers_[i].inUse) {
      ++count;
    }
  }

  return count;
#endif
}

uint8_t Kernel::queuedEventCount() const {
  return eventQueueCount_;
}

uint8_t Kernel::queuedCommandCount() const {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  return 0;
#else
  return commandQueueCount_;
#endif
}

uint8_t Kernel::queuedWorkCount() const {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  return 0;
#else
  return workQueueCount_;
#endif
}

bool Kernel::dropOldestEvent_() {
  if (eventQueueCount_ == 0) {
    return false;
  }

  eventQueue_[eventQueueHead_] = EventMessage();
  eventQueueHead_ = static_cast<uint8_t>((eventQueueHead_ + 1) % kEventQueueStorage);
  --eventQueueCount_;
  ++kernelStats_.queuedEventsDropped;
  emitSignal_(kSignalEventDrop, "event.queue", kernelStats_.queuedEventsDropped);
  return true;
}

bool Kernel::dropOldestCommand_() {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  return false;
#else
  if (commandQueueCount_ == 0) {
    return false;
  }

  commandQueue_[commandQueueHead_] = CommandMessage();
  commandQueueHead_ = static_cast<uint8_t>((commandQueueHead_ + 1) % kCommandQueueStorage);
  --commandQueueCount_;
  ++kernelStats_.queuedCommandsDropped;
  emitSignal_(kSignalCommandDrop, "command.queue", kernelStats_.queuedCommandsDropped);
  return true;
#endif
}

bool Kernel::dropOldestWork_() {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  return false;
#else
  if (workQueueCount_ == 0) {
    return false;
  }

  workQueue_[workQueueHead_] = WorkMessage();
  workQueueHead_ = static_cast<uint8_t>((workQueueHead_ + 1) % kWorkQueueStorage);
  --workQueueCount_;
  ++kernelStats_.queuedWorkDropped;
  emitSignal_(kSignalWorkDrop, "work.queue", kernelStats_.queuedWorkDropped);
  return true;
#endif
}

bool Kernel::enqueueEvent_(const char* topic, const EventValue& value) {
  if (topic == NULL) {
    return false;
  }

  return enqueueEventByKey_(topicKey_(topic), value);
}

bool Kernel::enqueueEventByKey_(TopicKey topicKey, const EventValue& value) {
  if (topicKey == 0U || kMaxEventQueue == 0) {
    return false;
  }

#if ZEROKERNEL_ENABLE_EVENT_COALESCING
  uint8_t scanIndex = eventQueueHead_;
  for (uint8_t i = 0; i < eventQueueCount_; ++i) {
    EventMessage& queued = eventQueue_[scanIndex];
    if (queued.inUse && queued.topicKey == topicKey && eventValuesEqual_(queued.value, value)) {
      return true;
    }
    scanIndex = static_cast<uint8_t>((scanIndex + 1) % kEventQueueStorage);
  }
#endif

  if (eventQueueCount_ >= kMaxEventQueue) {
#if ZEROKERNEL_EVENT_QUEUE_BACKPRESSURE == ZEROKERNEL_QUEUE_DROP_OLDEST
    if (!dropOldestEvent_()) {
      return false;
    }
#else
    return false;
#endif
  }

  EventMessage& event = eventQueue_[eventQueueTail_];
  event = EventMessage();
  event.inUse = true;
  event.topicKey = topicKey;
  event.value = value;

  eventQueueTail_ = static_cast<uint8_t>((eventQueueTail_ + 1) % kEventQueueStorage);
  ++eventQueueCount_;
  return true;
}

bool Kernel::enqueueCommand_(const char* command, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)command;
  (void)value;
  return false;
#else
  if (command == NULL) {
    return false;
  }

  return enqueueCommandByKey_(topicKey_(command), value);
#endif
}

bool Kernel::enqueueCommandByKey_(TopicKey commandKey, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)commandKey;
  (void)value;
  return false;
#else
  if (commandKey == 0U || kMaxCommandQueue == 0) {
    return false;
  }

#if ZEROKERNEL_ENABLE_EVENT_COALESCING
  uint8_t scanIndex = commandQueueHead_;
  for (uint8_t i = 0; i < commandQueueCount_; ++i) {
    CommandMessage& queued = commandQueue_[scanIndex];
    if (queued.inUse && queued.topicKey == commandKey &&
        eventValuesEqual_(queued.value, value)) {
      return true;
    }
    scanIndex = static_cast<uint8_t>((scanIndex + 1) % kCommandQueueStorage);
  }
#endif

  if (commandQueueCount_ >= kMaxCommandQueue) {
#if ZEROKERNEL_COMMAND_QUEUE_BACKPRESSURE == ZEROKERNEL_QUEUE_DROP_OLDEST
    if (!dropOldestCommand_()) {
      return false;
    }
#else
    return false;
#endif
  }

  CommandMessage& entry = commandQueue_[commandQueueTail_];
  entry = CommandMessage();
  entry.inUse = true;
  entry.topicKey = commandKey;
  entry.value = value;

  commandQueueTail_ = static_cast<uint8_t>((commandQueueTail_ + 1) % kCommandQueueStorage);
  ++commandQueueCount_;
  ++kernelStats_.commandsQueued;
  return true;
#endif
}

bool Kernel::enqueueWork_(WorkHandler handler, const EventValue& value) {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  (void)handler;
  (void)value;
  return false;
#else
  if (handler == NULL || kMaxWorkQueue == 0) {
    return false;
  }

#if ZEROKERNEL_ENABLE_EVENT_COALESCING
  uint8_t scanIndex = workQueueHead_;
  for (uint8_t i = 0; i < workQueueCount_; ++i) {
    WorkMessage& queued = workQueue_[scanIndex];
    if (queued.inUse && queued.handler == handler && eventValuesEqual_(queued.value, value)) {
      return true;
    }
    scanIndex = static_cast<uint8_t>((scanIndex + 1) % kWorkQueueStorage);
  }
#endif

  if (workQueueCount_ >= kMaxWorkQueue) {
#if ZEROKERNEL_WORK_QUEUE_BACKPRESSURE == ZEROKERNEL_QUEUE_DROP_OLDEST
    if (!dropOldestWork_()) {
      return false;
    }
#else
    return false;
#endif
  }

  WorkMessage& entry = workQueue_[workQueueTail_];
  entry = WorkMessage();
  entry.inUse = true;
  entry.handler = handler;
  entry.value = value;

  workQueueTail_ = static_cast<uint8_t>((workQueueTail_ + 1) % kWorkQueueStorage);
  ++workQueueCount_;
  ++kernelStats_.workQueued;
  return true;
#endif
}

void Kernel::resetEventQueue_() {
  for (uint8_t i = 0; i < kEventQueueStorage; ++i) {
    eventQueue_[i] = EventMessage();
  }

  eventQueueHead_ = 0;
  eventQueueTail_ = 0;
  eventQueueCount_ = 0;
}

void Kernel::resetCommandQueue_() {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  commandQueueHead_ = 0;
  commandQueueTail_ = 0;
  commandQueueCount_ = 0;
#else
  for (uint8_t i = 0; i < kCommandQueueStorage; ++i) {
    commandQueue_[i] = CommandMessage();
  }

  commandQueueHead_ = 0;
  commandQueueTail_ = 0;
  commandQueueCount_ = 0;
#endif
}

void Kernel::resetWorkQueue_() {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  workQueueHead_ = 0;
  workQueueTail_ = 0;
  workQueueCount_ = 0;
#else
  for (uint8_t i = 0; i < kWorkQueueStorage; ++i) {
    workQueue_[i] = WorkMessage();
  }

  workQueueHead_ = 0;
  workQueueTail_ = 0;
  workQueueCount_ = 0;
#endif
}

void Kernel::drainEventQueue_(uint8_t limit) {
  uint8_t drained = 0;

  while (eventQueueCount_ > 0) {
    if (limit != 0 && drained >= limit) {
      break;
    }

    EventMessage event = eventQueue_[eventQueueHead_];
    eventQueue_[eventQueueHead_] = EventMessage();
    eventQueueHead_ = static_cast<uint8_t>((eventQueueHead_ + 1) % kEventQueueStorage);
    --eventQueueCount_;
    ++drained;

    if (!event.inUse) {
      continue;
    }

    dispatchEventByKey_(event.topicKey, event.value);
  }
}

void Kernel::drainCommandQueue_(uint8_t limit) {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  (void)limit;
#else
  uint8_t drained = 0;

  while (commandQueueCount_ > 0) {
    if (limit != 0 && drained >= limit) {
      break;
    }

    CommandMessage command = commandQueue_[commandQueueHead_];
    commandQueue_[commandQueueHead_] = CommandMessage();
    commandQueueHead_ = static_cast<uint8_t>((commandQueueHead_ + 1) % kCommandQueueStorage);
    --commandQueueCount_;
    ++drained;

    if (!command.inUse) {
      continue;
    }

    dispatchCommandByKey_(command.topicKey, command.value);
  }
#endif
}

void Kernel::drainWorkQueue_(uint8_t limit) {
#if !ZEROKERNEL_ENABLE_WORK_QUEUE
  (void)limit;
#else
  uint8_t drained = 0;

  while (workQueueCount_ > 0) {
    if (limit != 0 && drained >= limit) {
      break;
    }

    WorkMessage work = workQueue_[workQueueHead_];
    workQueue_[workQueueHead_] = WorkMessage();
    workQueueHead_ = static_cast<uint8_t>((workQueueHead_ + 1) % kWorkQueueStorage);
    --workQueueCount_;
    ++drained;

    if (!work.inUse || work.handler == NULL) {
      continue;
    }

    work.handler(work.value);
    ++kernelStats_.workDelivered;
  }
#endif
}

int Kernel::findFreeSubscriberIndex_() const {
  for (uint8_t i = 0; i < kMaxSubscribers; ++i) {
    if (!subscribers_[i].inUse) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

int Kernel::findFreeTypedSubscriberIndex_() const {
#if !ZEROKERNEL_ENABLE_TYPED_EVENTS
  return -1;
#else
  for (uint8_t i = 0; i < kMaxTypedSubscribers; ++i) {
    if (!typedSubscribers_[i].inUse) {
      return static_cast<int>(i);
    }
  }

  return -1;
#endif
}

int Kernel::findFreeCommandHandlerIndex_() const {
#if !ZEROKERNEL_ENABLE_COMMAND_QUEUE
  return -1;
#else
  for (uint8_t i = 0; i < kMaxCommandHandlers; ++i) {
    if (!commandHandlers_[i].inUse) {
      return static_cast<int>(i);
    }
  }

  return -1;
#endif
}

}  // namespace zerokernel
