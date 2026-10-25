#include "EngineBootstrap.h"
#include "EngineRenderBackend.h"
#include "../../host/import_registry.h"

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
