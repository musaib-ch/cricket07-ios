/* Test the orchestration without loading game files or linking the engine. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/EngineBootstrap.h"

static int calls[8], count;
static int imports_ok = 1, recorder_ok = 1, memory_ok = 1;
static int loader_ok = 1, loader_calls;
static int translated_entry_ready;
static int registry_calls;
static int registry_register_entry = 1;
static void register_synthetic_functions(void) {
    ++registry_calls;
    /* Recursive registration must fail without calling the callback again. */
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    if (registry_register_entry) translated_entry_ready = 1;
}
#if defined(TEST_LINKED_REGISTRY)
void register_all_functions(void) { register_synthetic_functions(); }
#endif
static void synthetic_translated_entry(CPU *cpu) { cpu->eax = 0xC0707u; }
static void sentinel_entry(CPU *cpu) { (void)cpu; }
GuestFn lookup_function(uint32_t addr) {
    return translated_entry_ready && addr == 0x00401000u
        ? synthetic_translated_entry : NULL;
}
uint32_t g_image_lo, g_image_hi;

int load_image(const char *path, LoadedImage *out) {
    ++loader_calls;
    assert(strcmp(path, "/tmp/cricket07-synthetic.exe") == 0);
    if (!loader_ok) {
        /* Simulate a buggy loader modifying output before returning failure. */
        out->image_base = 0xdeadbeefu;
        return -1;
    }
    assert(!cricket07_engine_image_loaded());
    g_image_lo = 0x00400000u;
    g_image_hi = 0x00404000u;
    out->image_base = g_image_lo;
    out->entry = 0x00401000u;
    out->num_imports = 1;
    return 0;
}

uint32_t host_register_import_handlers(void) {
    calls[count++] = 1;
    return imports_ok ? 42u : 0u;
}
int cricket07_engine_select_ios_recorder(void) {
    calls[count++] = 2;
    return recorder_ok;
}

int runtime_try_init(CPU *c) {
    calls[count++] = 3;
    if (!memory_ok) return 0;
    memset(c, 0, sizeof *c);
    c->esp = 0x00300000u;
    return 1;
}

int main(void) {
    LoadedImage image = {0x11111111u, 0x22222222u, 0x33333333u};
    assert(!cricket07_engine_image_loaded());
    assert(!cricket07_engine_translated_functions_installed());
#if defined(TEST_LINKED_REGISTRY)
    assert(cricket07_engine_linked_translated_functions_available());
#else
    assert(!cricket07_engine_linked_translated_functions_available());
#endif
    assert(!cricket07_engine_install_linked_translated_functions());
    assert(!cricket07_engine_install_translated_functions(NULL));
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 0);
    GuestFn resolved = sentinel_entry;
    uint32_t entry_address = 0xfeedbeefu;
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    assert(resolved == sentinel_entry && entry_address == 0xfeedbeefu);
    assert(!cricket07_engine_load_user_executable(NULL, &image));
    assert(!cricket07_engine_load_user_executable("", &image));
    assert(!cricket07_engine_load_user_executable("relative.exe", &image));
    assert(!cricket07_engine_load_user_executable("/tmp/cricket07-synthetic.exe", NULL));
    assert(count == 0 && loader_calls == 0);
    assert(!cricket07_engine_guest_cpu());
    assert(!cricket07_engine_host_prepared());
#if defined(TEST_IMPORT_FAILURE)
    imports_ok = 0;
    assert(!cricket07_engine_prepare_host());
    assert(!cricket07_engine_host_prepared());
    assert(count == 1 && calls[0] == 1);
    imports_ok = 1;
    assert(cricket07_engine_prepare_host());
    assert(count == 3 && calls[1] == 1 && calls[2] == 2);
#elif defined(TEST_RECORDER_FAILURE)
    recorder_ok = 0;
    assert(!cricket07_engine_prepare_host());
    assert(!cricket07_engine_host_prepared());
    assert(count == 2 && calls[0] == 1 && calls[1] == 2);
    recorder_ok = 1;
    assert(cricket07_engine_prepare_host());
    assert(count == 4 && calls[2] == 1 && calls[3] == 2);
#else
    assert(cricket07_engine_prepare_host());
    assert(count == 2 && calls[0] == 1 && calls[1] == 2);
#endif
    assert(cricket07_engine_host_prepared());
    int before = count;
    assert(cricket07_engine_prepare_host());
    assert(count == before); /* no duplicate import/backend mutation */
#if defined(TEST_MEMORY_FAILURE)
    memory_ok = 0;
    assert(!cricket07_engine_load_user_executable("/tmp/cricket07-synthetic.exe", &image));
    assert(loader_calls == 0);
    assert(!cricket07_engine_prepare_guest());
    assert(!cricket07_engine_guest_cpu());
    assert(count == before + 2 && calls[before] == 3 && calls[before + 1] == 3);
    memory_ok = 1;
#endif
    assert(cricket07_engine_prepare_guest());
    assert(cricket07_engine_guest_cpu());
    assert(cricket07_engine_guest_cpu()->esp == 0x00300000u);
    int ready_count = count;
    assert(cricket07_engine_prepare_guest());
    assert(count == ready_count); /* do not remap or reset the guest CPU */

    loader_ok = 0;
    assert(!cricket07_engine_load_user_executable("/tmp/cricket07-synthetic.exe", &image));
    assert(loader_calls == 1 && !cricket07_engine_image_loaded());
    assert(image.image_base == 0x11111111u && image.entry == 0x22222222u);
    assert(image.num_imports == 0x33333333u);
    loader_ok = 1;
    assert(cricket07_engine_load_user_executable("/tmp/cricket07-synthetic.exe", &image));
    assert(loader_calls == 2 && cricket07_engine_image_loaded());
    assert(image.image_base == 0x00400000u && image.entry == 0x00401000u);
    assert(image.num_imports == 1);
    /* Loading alone cannot imply translated entrypoint readiness. */
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    assert(resolved == sentinel_entry && entry_address == 0xfeedbeefu);
    translated_entry_ready = 1;
    assert(!cricket07_engine_resolve_loaded_entrypoint(NULL, &entry_address));
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, NULL));
    assert(resolved == sentinel_entry && entry_address == 0xfeedbeefu);
    /* Reject stale or out-of-range image metadata even if a function exists. */
    g_image_hi = 0x00400080u;
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    g_image_hi = 0x00404000u;
    g_image_lo = 0x00500000u;
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    g_image_lo = 0x00400000u;
    assert(resolved == sentinel_entry && entry_address == 0xfeedbeefu);
    assert(cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    assert(resolved == synthetic_translated_entry && entry_address == 0x00401000u);
    translated_entry_ready = 0;
    assert(!cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    assert(resolved == synthetic_translated_entry && entry_address == 0x00401000u);
    assert(!cricket07_engine_load_user_executable("/tmp/cricket07-synthetic.exe", &image));
    assert(loader_calls == 2); /* never reload over an active image */
    assert(count == ready_count); /* loader must not reset host or guest */
    /* Invalid image bounds must not consume the one-shot registration. */
    g_image_hi = 0x00400080u;
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 0);
    g_image_hi = 0x00404000u;
#if defined(TEST_LINKED_REGISTRY)
    assert(cricket07_engine_install_linked_translated_functions());
    assert(registry_calls == 1 && cricket07_engine_translated_functions_installed());
    assert(!cricket07_engine_install_linked_translated_functions());
    assert(registry_calls == 1);
#elif defined(TEST_REGISTRY_FAILURE)
    registry_register_entry = 0;
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 1 && !cricket07_engine_translated_functions_installed());
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 1); /* never retry a partially mutated registry */
#else
    /* A missing weak-linked symbol must not consume the direct registration. */
    assert(!cricket07_engine_install_linked_translated_functions());
    assert(cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 1 && cricket07_engine_translated_functions_installed());
    assert(!cricket07_engine_install_translated_functions(register_synthetic_functions));
    assert(registry_calls == 1); /* no duplicate generated registration */
    assert(cricket07_engine_resolve_loaded_entrypoint(&resolved, &entry_address));
    assert(resolved == synthetic_translated_entry && entry_address == 0x00401000u);
#endif
    return 0;
}
