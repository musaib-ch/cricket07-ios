/* Shared import registration for the desktop launcher and a future
 * SDL-free iOS bootstrap. Do not duplicate or truncate import tables.
 */
#include "import_registry.h"
#include "win32.h"

#define HOST_MAX_IMPORT_HANDLERS 1024u

uint32_t host_register_import_handlers(void) {
    static ImportHandler handlers[HOST_MAX_IMPORT_HANDLERS];
    const ImportTable *tables[] = {
        &kernel32_imports, &user32_imports, &gdi32_imports,
        &advapi32_imports, &winmm_imports, &directx_imports
    };
    uint32_t total = 0;

    /* Validate all tables before modifying the registered handler set. */
    for (size_t i = 0; i < sizeof tables / sizeof tables[0]; ++i) {
        const ImportTable *table = tables[i];
        if (!table || (table->count && !table->list) ||
            table->count > HOST_MAX_IMPORT_HANDLERS - total)
            return 0;
        total += table->count;
    }
    if (!total) return 0;
    uint32_t offset = 0;
    for (size_t i = 0; i < sizeof tables / sizeof tables[0]; ++i) {
        const ImportTable *table = tables[i];
        if (table->count) {
            memcpy(handlers + offset, table->list,
                   (size_t)table->count * sizeof handlers[0]);
            offset += table->count;
        }
    }
    set_import_handlers(handlers, total);
    return total;
}
