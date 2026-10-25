#ifndef CRICKET07_HOST_IMPORT_REGISTRY_H
#define CRICKET07_HOST_IMPORT_REGISTRY_H

#include <stdint.h>

/* Install the six existing Win32/DirectX import tables into the translated
 * runtime. Returns the number of handlers installed, or zero on validation
 * failure. Call before load_image(), on the single engine startup thread.
 * Does not initialize guest memory, open a window, or execute game code.
 */
uint32_t host_register_import_handlers(void);

#endif
