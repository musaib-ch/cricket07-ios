#ifndef CRICKET07_CONTROLLER_INPUT_PUMP_H
#define CRICKET07_CONTROLLER_INPUT_PUMP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Call only on the translated game's input/event thread. The callback should
 * forward each HID usage to host/input.c's input_key_hid(usage, pressed).
 * The diagnostic iOS shell does not start that game thread yet. */
typedef void (*Cricket07HIDSink)(uint16_t usage, int pressed, void *context);

/* Dispatch at most min(max_events, 32) queued transitions in FIFO order.
 * The callback runs without the queue mutex held, so it can safely enqueue.
 * A NULL callback or zero budget does not consume any input. */
size_t cricket07_controller_pump(Cricket07HIDSink sink, void *context,
                                 size_t max_events);
#ifdef __cplusplus
}
#endif
#endif
