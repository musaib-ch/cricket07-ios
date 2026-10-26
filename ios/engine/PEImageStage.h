#ifndef CRICKET07_PE_IMAGE_STAGE_H
#define CRICKET07_PE_IMAGE_STAGE_H

#include <stddef.h>
#include <stdint.h>
#include "PEPreflight.h"
#include "PEImportPlan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Build a complete, zero-initialized PE image with all IAT entries patched
 * in private memory. No guest-memory or dispatch-table writes occur here.
 * On success, caller owns *image and must free it. On failure *image is
 * untouched. The import plan must come from cricket07_pe32_stage_imports().
 * The caller must hold the single-engine-thread registration invariant. */
int cricket07_pe32_stage_image(const uint8_t *file, size_t length,
                              const Cricket07PEInfo *info,
                              const Cricket07PEImportPlan *imports,
                              uint32_t occupied_slots, uint32_t import_base,
                              uint8_t **image);

#ifdef __cplusplus
}
#endif
#endif
