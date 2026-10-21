#ifndef CRICKET07_RUNTIME_BRIDGE_H
#define CRICKET07_RUNTIME_BRIDGE_H
#include <stdint.h>
/// Checks that the native application can resolve a symbol from the upstream C runtime.
/// Does not initialize guest memory, load the executable, or start gameplay.
int cricket07_runtime_linked(void);
#endif
