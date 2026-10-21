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

#if defined(__APPLE__)
/// Exercises the guest CPU stack and TIB layout in a disposable mapping.
/// Must run before any real game runtime is started: g_mem is process-global.
int cricket07_cpu_stack_probe(void) {
    if (g_mem != NULL) return 0;
    const size_t bytes = (size_t)GUEST_SPACE + 0x10000u;
    uint8_t *mapping = mmap(NULL, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return 0;
    const size_t page = (size_t)getpagesize();
    uintptr_t tib_page = (uintptr_t)(mapping + GUEST_TIB);
    uintptr_t stack_page = (uintptr_t)(mapping + GUEST_STACK_TOP - page);
    int ok = mprotect((void *)tib_page, page, PROT_READ | PROT_WRITE) == 0;
    if (ok) ok = mprotect((void *)stack_page, page, PROT_READ | PROT_WRITE) == 0;
    if (ok) {
        g_mem = mapping;
        CPU cpu = {0};
        cpu.esp = GUEST_STACK_TOP;
        cpu.fpu_cw = 0x027f;
        cpu.mxcsr = 0x1f80;
        cpu.fs_base = GUEST_TIB;
        W32(GUEST_TIB, 0xffffffffu);
        W32(GUEST_TIB + 0x18, GUEST_TIB);
        W32(GUEST_TIB + 0x04, GUEST_STACK_TOP);
        W32(GUEST_TIB + 0x08, GUEST_STACK_TOP - GUEST_STACK_SIZE);
        PUSH32(&cpu, 0xC0707u);
        ok = cpu.esp == GUEST_STACK_TOP - 4 &&
             POP32(&cpu) == 0xC0707u &&
             cpu.esp == GUEST_STACK_TOP &&
             R32(GUEST_TIB + 0x18) == GUEST_TIB &&
             R32(GUEST_TIB + 0x08) == GUEST_STACK_TOP - GUEST_STACK_SIZE;
        g_mem = NULL;
    }
    munmap(mapping, bytes);
    return ok;
}
#else
int cricket07_cpu_stack_probe(void) { return 0; }
#endif
