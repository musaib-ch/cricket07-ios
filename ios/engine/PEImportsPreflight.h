#ifndef CRICKET07_PE_IMPORTS_PREFLIGHT_H
#define CRICKET07_PE_IMPORTS_PREFLIGHT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only validation of PE32 import metadata. Not a replacement for
 * bounds checks in the guest-memory loader. */
int cricket07_pe32_imports_preflight(const uint8_t *bytes, size_t length,
                                    uint32_t *import_count);
#ifdef __cplusplus
}
#endif
#endif
