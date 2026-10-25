#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../host/win32.h"
#include "../host/import_registry.h"

static void noop(CPU *cpu) { (void)cpu; }
#if !defined(TEST_NULL_TABLE)
static const ImportHandler k[] = { { "KERNEL32!One", noop } };
#endif
static const ImportHandler u[] = { { "USER32!Two", noop } };
static const ImportHandler g[] = { { "GDI32!Three", noop } };
static const ImportHandler a[] = { { "ADVAPI32!Four", noop } };
static const ImportHandler w[] = { { "WINMM!Five", noop } };
static const ImportHandler d[] = { { "DIRECTX!Six", noop } };

#if defined(TEST_OVERFLOW)
const ImportTable kernel32_imports = { k, 1025 };
#elif defined(TEST_NULL_TABLE)
const ImportTable kernel32_imports = { NULL, 1 };
#else
const ImportTable kernel32_imports = { k, 1 };
#endif
const ImportTable user32_imports = { u, 1 };
const ImportTable gdi32_imports = { g, 1 };
const ImportTable advapi32_imports = { a, 1 };
const ImportTable winmm_imports = { w, 1 };
const ImportTable directx_imports = { d, 1 };

static int calls;
static uint32_t observed;
static const ImportHandler *received;
void set_import_handlers(ImportHandler *handlers, uint32_t count) {
    ++calls;
    observed = count;
    received = handlers;
}

int main(void) {
#if defined(TEST_OVERFLOW) || defined(TEST_NULL_TABLE)
    assert(host_register_import_handlers() == 0);
    assert(calls == 0);
#else
    assert(host_register_import_handlers() == 6);
    assert(calls == 1 && observed == 6 && received != NULL);
    const char *expected[] = {
        "KERNEL32!One", "USER32!Two", "GDI32!Three",
        "ADVAPI32!Four", "WINMM!Five", "DIRECTX!Six"
    };
    for (size_t i = 0; i < 6; ++i) {
        assert(strcmp(received[i].name, expected[i]) == 0);
        assert(received[i].fn == noop);
    }
    assert(host_register_import_handlers() == 6);
    assert(calls == 2);
#endif
    puts("Shared host import registry tests passed");
    return 0;
}
