#ifndef CRICKET07_PE_PREFLIGHT_H
#define CRICKET07_PE_PREFLIGHT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only validation for the existing x86 PE32 loader. Not a safe loader:
 * load_image() still requires internal import-table bounds checks. */
typedef struct {
    uint32_t image_base, image_size, entry_rva, import_rva;
    uint16_t section_count;
} Cricket07PEInfo;
int cricket07_pe32_preflight(const uint8_t *bytes, size_t length,
                            Cricket07PEInfo *out);
#ifdef __cplusplus
}
#endif
#endif
