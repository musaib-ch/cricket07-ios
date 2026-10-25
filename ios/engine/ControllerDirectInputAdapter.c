#include "ControllerDirectInputAdapter.h"
#include "ControllerInputPump.h"
#include <stdint.h>

/* Provided by host/input.c. No fallback is permitted: the eventual engine
 * target must link the real DirectInput host implementation. */
extern void input_key_hid(int usage, int down);

static void deliver_to_directinput(uint16_t usage, int pressed, void *context) {
    (void)context;
    input_key_hid((int)usage, pressed);
}

size_t cricket07_controller_pump_directinput(size_t max_events) {
    return cricket07_controller_pump(deliver_to_directinput, NULL, max_events);
}
