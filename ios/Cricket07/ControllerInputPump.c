#include "ControllerInputPump.h"
#include "ControllerInputQueue.h"

#define CRICKET07_INPUT_PUMP_BATCH 32u

size_t cricket07_controller_pump(Cricket07HIDSink sink, void *context,
                                 size_t max_events) {
    Cricket07HIDEvent events[CRICKET07_INPUT_PUMP_BATCH];
    if (!sink || !max_events) return 0;
    if (max_events > CRICKET07_INPUT_PUMP_BATCH)
        max_events = CRICKET07_INPUT_PUMP_BATCH;

    /* Drain under the queue's mutex; invoke game input outside the lock. */
    size_t count = cricket07_controller_drain(events, max_events);
    for (size_t i = 0; i < count; ++i)
        sink(events[i].hid_usage, events[i].pressed != 0, context);
    return count;
}
