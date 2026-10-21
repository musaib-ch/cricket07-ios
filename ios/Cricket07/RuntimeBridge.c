#include "RuntimeBridge.h"
#include "runtime.h"

static void smoke_guest_function(CPU *cpu) {
    cpu->eax = 0xC07;
}

int cricket07_runtime_linked(void) {
    return lookup_function(0) == 0;
}

int cricket07_runtime_registry_smoke_test(void) {
    const uint32_t address = 0x00401234u;
    if (lookup_function(address) != 0) return 0;
    register_function(address, smoke_guest_function);
    GuestFn function = lookup_function(address);
    if (function != smoke_guest_function) return 0;
    CPU cpu = {0};
    function(&cpu);
    return cpu.eax == 0xC07;
}
