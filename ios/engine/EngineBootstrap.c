#include "EngineBootstrap.h"
#include "EngineRenderBackend.h"
#include "../../host/import_registry.h"
#include "../../runtime/runtime.h"

/* Owned exclusively by the future engine startup thread, not atomic. */
static int g_host_prepared;

int cricket07_engine_prepare_host(void) {
    if (g_host_prepared) return 1;
    if (!host_register_import_handlers()) return 0;
    if (!cricket07_engine_select_ios_recorder()) return 0;
    g_host_prepared = 1;
    return 1;
}

int cricket07_engine_host_prepared(void) {
    return g_host_prepared;
}

/* The CPU remains valid for the lifetime of the process. No UI-thread calls. */
static CPU g_guest_cpu;
static int g_guest_ready;
/* Published only after a successful image load on the startup thread. */
static LoadedImage g_loaded_image;
static int g_loaded_image_valid;

int cricket07_engine_prepare_guest(void) {
    if (g_guest_ready) return 1;
    if (!cricket07_engine_prepare_host()) return 0;
    if (!runtime_try_init(&g_guest_cpu)) return 0;
    g_guest_ready = 1;
    return 1;
}

CPU *cricket07_engine_guest_cpu(void) {
    return g_guest_ready ? &g_guest_cpu : NULL;
}

/* One-shot image loading is deliberately separate from translated execution. */
int cricket07_engine_image_loaded(void) {
    return g_image_hi > g_image_lo;
}

int cricket07_engine_load_user_executable(const char *absolute_path, LoadedImage *out) {
    /* Do not initialize the guest or open files for invalid paths/callers. */
    if (!absolute_path || absolute_path[0] != '/' || !absolute_path[1] ||
        !out || cricket07_engine_image_loaded()) return 0;
    if (!cricket07_engine_prepare_guest()) return 0;

    /* Preserve the caller's metadata even if a loader error modifies its
       output parameter. The loader owns guest-memory publication. */
    LoadedImage staged;
    if (load_image(absolute_path, &staged) != 0) return 0;
    g_loaded_image = staged;
    g_loaded_image_valid = 1;
    *out = staged;
    return 1;
}

/* Resolve but never invoke the generated entrypoint. No callbacks are
 * published to callers until the image and function registry agree. */
int cricket07_engine_resolve_loaded_entrypoint(GuestFn *out_fn, uint32_t *out_address) {
    if (!out_fn || !out_address || !g_guest_ready || !g_loaded_image_valid ||
        !cricket07_engine_image_loaded()) return 0;
    if (g_loaded_image.image_base != g_image_lo ||
        g_loaded_image.entry < g_image_lo ||
        g_loaded_image.entry >= g_image_hi) return 0;
    GuestFn fn = lookup_function(g_loaded_image.entry);
    if (!fn) return 0;
    *out_fn = fn;
    *out_address = g_loaded_image.entry;
    return 1;
}


/* A generated registry may register many functions before discovering a
 * missing entry point. Treat its callback as a one-shot mutation, never a
 * retryable operation. No translated game function is executed here. */
static int g_registry_install_attempted;
static int g_registry_entry_verified;

int cricket07_engine_install_translated_functions(void (*registrar)(void)) {
    if (!registrar || g_registry_install_attempted || !g_guest_ready ||
        !g_loaded_image_valid || !cricket07_engine_image_loaded()) return 0;
    /* Reject inconsistent image metadata before invoking user-supplied code. */
    if (g_loaded_image.image_base != g_image_lo ||
        g_loaded_image.entry < g_image_lo ||
        g_loaded_image.entry >= g_image_hi) return 0;

    /* Set before invocation to reject reentrant registration. If the callback
     * fails to register an entry, partial registrations are retained and a
     * second attempt is forbidden for the lifetime of this guest. */
    g_registry_install_attempted = 1;
    registrar();
    GuestFn entry = NULL;
    uint32_t address = 0;
    if (!cricket07_engine_resolve_loaded_entrypoint(&entry, &address)) return 0;
    g_registry_entry_verified = 1;
    return 1;
}

int cricket07_engine_translated_functions_installed(void) {
    return g_registry_entry_verified;
}

/* The generated registry is intentionally absent from this public source tree.
 * A weak reference allows the engine archive to link without game-derived
 * objects. A final application must explicitly link the generated object;
 * the weak reference alone will not pull it out of a static archive.
 */
#if defined(__APPLE__)
extern void register_all_functions(void) __attribute__((weak_import));
#elif defined(__GNUC__) || defined(__clang__)
extern void register_all_functions(void) __attribute__((weak));
#else
#error "Linked translated registry probe requires weak symbol support"
#endif

int cricket07_engine_linked_translated_functions_available(void) {
    return register_all_functions != NULL;
}

int cricket07_engine_install_linked_translated_functions(void) {
    if (!cricket07_engine_linked_translated_functions_available()) return 0;
    return cricket07_engine_install_translated_functions(register_all_functions);
}
