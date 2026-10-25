#ifndef CRICKET07_CONTROLLER_INPUT_QUEUE_H
#define CRICKET07_CONTROLLER_INPUT_QUEUE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Small, thread-safe bridge from GameController virtual keys to USB HID usages.
 * The translated host/input.c accepts HID usages through input_key_hid(), but
 * must consume these events on the game thread, never from a Swift callback.
 * This queue does NOT start the game or deliver events to DirectInput yet. */
typedef struct {
    uint16_t hid_usage;
    uint8_t pressed;
    uint8_t reserved;
} Cricket07HIDEvent;

/* Returns 1 when a state change is queued; 0 for unknown/duplicate keys or
 * overflow. On overflow, pending events are replaced with key releases so
 * a later game-thread consumer cannot retain a permanently held button. */
int cricket07_controller_enqueue_vk(uint16_t virtual_key, int pressed);

/* Consume up to capacity events in FIFO order. No heap allocation. */
size_t cricket07_controller_drain(Cricket07HIDEvent *events, size_t capacity);

/* Clear queued input and publish releases for every currently held key. */
void cricket07_controller_reset(void);

#ifdef __cplusplus
}
#endif
#endif
