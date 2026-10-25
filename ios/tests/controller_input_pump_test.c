#include "ControllerInputPump.h"
#include "ControllerInputQueue.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    pthread_t owner;
    uint16_t usages[64];
    int pressed[64];
    size_t count;
    int reenter;
} Capture;

static void host_input_stub(uint16_t usage, int pressed, void *opaque) {
    Capture *capture = (Capture *)opaque;
    assert(pthread_equal(pthread_self(), capture->owner));
    assert(capture->count < 64);
    capture->usages[capture->count] = usage;
    capture->pressed[capture->count++] = pressed;
    /* A callback may enqueue without deadlocking the input queue. */
    if (capture->reenter && usage == 80 && pressed)
        assert(cricket07_controller_enqueue_vk(0x27u, 1) == 1);
}

static void *game_thread(void *opaque) {
    Capture *capture = (Capture *)opaque;
    capture->owner = pthread_self();
    assert(cricket07_controller_pump(NULL, capture, 8) == 0);
    assert(cricket07_controller_pump(host_input_stub, capture, 0) == 0);
    assert(cricket07_controller_pump(host_input_stub, capture, 1) == 1);
    assert(capture->count == 1 && capture->usages[0] == 80 &&
           capture->pressed[0] == 1);
    /* The Enter event was queued before the callback added Right. */
    assert(cricket07_controller_pump(host_input_stub, capture, 64) == 2);
    assert(capture->usages[1] == 40 && capture->pressed[1] == 1);
    assert(capture->usages[2] == 79 && capture->pressed[2] == 1);
    assert(cricket07_controller_pump(host_input_stub, capture, 64) == 0);
    return NULL;
}

int main(void) {
    Cricket07HIDEvent discarded[8];
    cricket07_controller_reset();
    assert(cricket07_controller_drain(discarded, 8) == 6);
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 1);
    assert(cricket07_controller_enqueue_vk(0x0du, 1) == 1);
    Capture capture = {0};
    capture.reenter = 1;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, game_thread, &capture) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(capture.count == 3);

    /* Reset emits releases, dispatched on the caller's thread. */
    cricket07_controller_reset();
    capture.owner = pthread_self();
    capture.reenter = 0;
    assert(cricket07_controller_pump(host_input_stub, &capture, 64) == 6);
    assert(capture.count == 9);
    for (size_t i = 3; i < capture.count; ++i)
        assert(capture.pressed[i] == 0);
    puts("controller input game-thread pump tests passed");
    return 0;
}
