#ifndef CRICKET07_PE_IMPORT_PLAN_H
#define CRICKET07_PE_IMPORT_PLAN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CRICKET07_PE_MAX_STAGED_IMPORTS 4096u
typedef struct {
    uint32_t iat_rva;
    char name[96];
} Cricket07PEStagedImport;
typedef struct {
    uint32_t count;
    Cricket07PEStagedImport entries[CRICKET07_PE_MAX_STAGED_IMPORTS];
} Cricket07PEImportPlan;
/* Stage names and IAT destinations from immutable PE file bytes before
 * guest-memory mutation. A failed stage has count=0 and must not be applied.
 * Caller owns the plan storage (allocate on heap for iOS). */
int cricket07_pe32_stage_imports(const uint8_t *file, size_t length,
                                uint32_t occupied_slots,
                                Cricket07PEImportPlan *plan);
#ifdef __cplusplus
}
#endif
#endif
