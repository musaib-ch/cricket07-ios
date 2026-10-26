/* Exercises the real runtime import registration, without game assets. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../runtime/runtime.h"

static void dummy(CPU *c) { (void)c; }

int main(void) {
    assert(runtime_import_slots_remaining() == 4096u);
    assert(add_dynamic_import(NULL, dummy) == 0);
    assert(add_dynamic_import("", dummy) == 0);
    assert(add_dynamic_import("KERNEL32.DLL!Test", NULL) == 0);

    char too_long[97];
    memset(too_long, 'x', 96);
    too_long[96] = 0;
    assert(add_dynamic_import(too_long, dummy) == 0);
    assert(runtime_import_slots_remaining() == 4096u);

    char longest[96];
    memset(longest, 'L', 95);
    longest[95] = 0;
    uint32_t first = add_dynamic_import(longest, dummy);
    assert(first == IMPORT_BASE);
    assert(!strcmp(import_name(first), longest));
    assert(lookup_function(first) == dummy);
    assert(runtime_import_slots_remaining() == 4095u);

    for (uint32_t i = 1; i < 4096u; ++i) {
        char name[48];
        snprintf(name, sizeof name, "SYNTHETIC.DLL!Method%u", i);
        uint32_t address = add_dynamic_import(name, dummy);
        assert(address == IMPORT_BASE + 16u * i);
        assert(lookup_function(address) == dummy);
        assert(!strcmp(import_name(address), name));
    }
    assert(runtime_import_slots_remaining() == 0);
    assert(add_dynamic_import("SYNTHETIC.DLL!Overflow", dummy) == 0);
    assert(runtime_import_slots_remaining() == 0);
    assert(import_name(IMPORT_BASE + 4096u * 16u) == NULL);
    assert(lookup_function(IMPORT_BASE + 4096u * 16u) == NULL);
    return 0;
}
