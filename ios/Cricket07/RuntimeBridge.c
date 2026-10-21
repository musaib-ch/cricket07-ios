#include "RuntimeBridge.h"
#include "runtime.h"

int cricket07_runtime_linked(void) {
    return lookup_function(0) == 0;
}
