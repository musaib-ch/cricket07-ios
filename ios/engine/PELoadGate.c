#include "PELoadGate.h"
#include "PEImportsPreflight.h"

#define CRICKET07_MAX_IMPORT_SLOTS 4096u

int cricket07_pe32_load_gate(const uint8_t *bytes, size_t length,
                            uint32_t occupied_import_slots,
                            Cricket07PEInfo *out) {
    Cricket07PEInfo info;
    uint32_t imports;
    if (!out || occupied_import_slots > CRICKET07_MAX_IMPORT_SLOTS ||
        !cricket07_pe32_preflight(bytes, length, &info) ||
        !cricket07_pe32_imports_preflight(bytes, length, &imports) ||
        imports > CRICKET07_MAX_IMPORT_SLOTS - occupied_import_slots)
        return 0;
    *out = info;
    return 1;
}
