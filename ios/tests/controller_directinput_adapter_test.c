#include "ControllerDirectInputAdapter.h"
#include "ControllerInputQueue.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static pthread_t expected_thread;
static int usages[32], states[32];
static size_t delivered;

/* Link-time stub matching host/input.c's actual exported symbol. */
void input_key_hid(int usage, int down) {
    assert(pthread_equal(pthread_self(), expected_thread));
    assert(delivered < 32);
    usages[delivered] = usage;
    states[delivered++] = down;
}

static void *game_input_thread(void *arg) {
    (void)arg;
    expected_thread = pthread_self();
    assert(cricket07_controller_pump_directinput(0) == 0);
    assert(cricket07_controller_pump_directinput(1) == 1);
    assert(delivered == 1 && usages[0] == 80 && states[0] == 1);
    assert(cricket07_controller_pump_directinput(64) == 2);
    assert(delivered == 3);
    assert(usages[1] == 40 && states[1] == 1);
    assert(usages[2] == 80 && states[2] == 0);
    assert(cricket07_controller_pump_directinput(64) == 0);
    return NULL;
}

int main(void) {
    Cricket07HIDEvent discarded[16];
    cricket07_controller_reset();
    assert(cricket07_controller_drain(discarded, 16) == 6);
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 1);
    assert(cricket07_controller_enqueue_vk(0x0du, 1) == 1);
    assert(cricket07_controller_enqueue_vk(0x25u, 0) == 1);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, game_input_thread, NULL) == 0);
    assert(pthread_join(thread, NULL) == 0);
    cricket07_controller_reset();
    expected_thread = pthread_self();
    assert(cricket07_controller_pump_directinput(32) == 6);
    assert(delivered == 9);
    for (size_t i = 3; i < delivered; ++i) assert(states[i] == 0);
    puts("controller DirectInput adapter tests passed");
    return 0;
}
