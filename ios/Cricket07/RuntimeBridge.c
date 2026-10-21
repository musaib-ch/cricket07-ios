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

#if defined(__APPLE__)
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/// Non-destructive reservation probe. Does not call runtime_init(), which exits
/// on failure, and does not touch the entire guest mapping.
int cricket07_guest_memory_probe(void) {
    const size_t guest_bytes = (size_t)UINT64_C(0x100010000);
    void *base = mmap(NULL, guest_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) return 0;
    // Touch only one page at each end, avoiding 4 GiB physical allocation.
    const size_t page = (size_t)getpagesize();
    uint8_t *last = (uint8_t *)base + guest_bytes - page;
    int ok = mprotect(base, page, PROT_READ | PROT_WRITE) == 0;
    if (ok) ok = mprotect(last, page, PROT_READ | PROT_WRITE) == 0;
    if (ok) {
        ((volatile uint8_t *)base)[0] = 0x07;
        ((volatile uint8_t *)last)[0] = 0x20;
        ok = ((volatile uint8_t *)base)[0] == 0x07 &&
             ((volatile uint8_t *)last)[0] == 0x20;
    }
    munmap(base, guest_bytes);
    return ok;
}
#else
int cricket07_guest_memory_probe(void) { return 0; }
#endif
