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

