#ifndef CRICKET07_CONTROLLER_DIRECTINPUT_ADAPTER_H
#define CRICKET07_CONTROLLER_DIRECTINPUT_ADAPTER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Call only on the translated game's input/event thread after the host
 * input subsystem has initialized. The diagnostic app does not call this. */
size_t cricket07_controller_pump_directinput(size_t max_events);
#ifdef __cplusplus
}
#endif
#endif
