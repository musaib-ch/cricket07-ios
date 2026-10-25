#include "ControllerInputQueue.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static Cricket07HIDEvent out[256];
static size_t drain(void) {
    return cricket07_controller_drain(out, sizeof(out) / sizeof(out[0]));
}
int main(void) {
    cricket07_controller_reset();
    assert(drain() == 0);
    assert(cricket07_controller_enqueue_vk(0x99u, 1) == 0);
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 1);
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 0);
    assert(cricket07_controller_enqueue_vk(0x0du, 1) == 1);
    assert(cricket07_controller_enqueue_vk(0x25u, 0) == 1);
    assert(drain() == 3);
    assert(out[0].hid_usage == 80 && out[0].pressed == 1);
    assert(out[1].hid_usage == 40 && out[1].pressed == 1);
    assert(out[2].hid_usage == 80 && out[2].pressed == 0);
    cricket07_controller_reset();
    assert(drain() == 1 && out[0].hid_usage == 40 && !out[0].pressed);

    const uint16_t vk[] = {0x25, 0x27, 0x26, 0x28, 0x0d, 0x1b};
    const uint16_t hid[] = {80, 79, 82, 81, 40, 41};
    for (size_t i = 0; i < 6; ++i)
        assert(cricket07_controller_enqueue_vk(vk[i], 1) == 1);
    assert(drain() == 6);
    for (size_t i = 0; i < 6; ++i)
        assert(out[i].hid_usage == hid[i] && out[i].pressed == 1);
    cricket07_controller_reset();
    assert(drain() == 6);
    for (size_t i = 0; i < 6; ++i)
        assert(out[i].hid_usage == hid[i] && out[i].pressed == 0);

    /* A saturated queue fails closed: held keys are released, not stuck. */
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 1);
    for (int i = 0; i < 127; ++i)
        assert(cricket07_controller_enqueue_vk(0x0du, (i & 1) == 0) == 1);
    assert(cricket07_controller_enqueue_vk(0x25u, 0) == 0);
    assert(drain() == 2);
    assert(out[0].hid_usage == 80 && out[0].pressed == 0);
    assert(out[1].hid_usage == 40 && out[1].pressed == 0);
    assert(cricket07_controller_enqueue_vk(0x25u, 0) == 0);
    assert(cricket07_controller_enqueue_vk(0x25u, 1) == 1);
    assert(drain() == 1 && out[0].hid_usage == 80 && out[0].pressed == 1);
    cricket07_controller_reset();
    assert(drain() == 1 && out[0].hid_usage == 80 && !out[0].pressed);
    assert(cricket07_controller_drain(NULL, 1) == 0);
    assert(cricket07_controller_drain(out, 0) == 0);
    puts("controller HID queue tests passed");
    return 0;
}
