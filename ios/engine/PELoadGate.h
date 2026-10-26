#ifndef CRICKET07_PE_LOAD_GATE_H
#define CRICKET07_PE_LOAD_GATE_H
#include "PEPreflight.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Read-only gate for the iOS PE loader. Checks the full image, import
 * metadata and remaining fixed import slots before guest memory is touched.
 * Does not load or execute code. Failure leaves *out untouched.
 * Call only on the single engine startup thread.
 */
int cricket07_pe32_load_gate(const uint8_t *bytes, size_t length,
                            uint32_t occupied_import_slots,
                            Cricket07PEInfo *out);
#ifdef __cplusplus
}
#endif
#endif
