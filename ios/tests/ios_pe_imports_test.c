/* Synthetic PE32 fixtures only; no proprietary game files. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/PEImportsPreflight.h"
static void w16(uint8_t *p, uint16_t n) { p[0] = n & 255; p[1] = n >> 8; }
static void w32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (i * 8));
}
static void fixture(uint8_t *p) {
    memset(p, 0, 4096);
    w16(p, 0x5a4d); w32(p + 0x3c, 0x80);
    uint8_t *pe = p + 0x80;
    w32(pe, 0x4550); w16(pe + 4, 0x14c);
    w16(pe + 6, 2); w16(pe + 20, 224);
    uint8_t *opt = pe + 24;
    w16(opt, 0x10b); w32(opt + 16, 0x1000);
    w32(opt + 28, 0x400000); w32(opt + 32, 0x1000);
    w32(opt + 36, 0x200); w32(opt + 56, 0x4000);
    w32(opt + 60, 0x200); w32(opt + 92, 16);
    w32(opt + 104, 0x2000); w32(opt + 108, 0x28);
    uint8_t *sec = opt + 224;
    w32(sec + 8, 0x1000); w32(sec + 12, 0x1000);
    w32(sec + 16, 0x200); w32(sec + 20, 0x200);
    w32(sec + 36, 0x60000020);
    sec += 40;
    w32(sec + 8, 0x1000); w32(sec + 12, 0x2000);
    w32(sec + 16, 0x200); w32(sec + 20, 0x400);
    w32(sec + 36, 0xc0000040);
    /* Import directory 0x2000 => file offset 0x400. */
    w32(p + 0x400, 0x2040);  /* ILT */
    w32(p + 0x40c, 0x2060);  /* DLL */
    w32(p + 0x410, 0x2050);  /* IAT */
    w32(p + 0x440, 0x2070);  /* name import */
    w32(p + 0x450, 0x2070);  /* IAT */
    memcpy(p + 0x460, "KERNEL32.dll", 13);
    memcpy(p + 0x472, "ExitProcess", 12); /* 2-byte hint at 0x470 */
}
static void bad(const uint8_t *p) {
    uint32_t count = 0x12345678;
    assert(!cricket07_pe32_imports_preflight(p, 4096, &count));
    assert(count == 0x12345678);
}
int main(void) {
    uint8_t p[4096]; uint32_t count = 0;
    fixture(p);
    assert(cricket07_pe32_imports_preflight(p, sizeof p, &count) && count == 1);
    bad(NULL);
    fixture(p); w32(p + 0x40c, 0x2fff); bad(p); /* unmapped DLL */
    fixture(p); w32(p + 0x400, 0x2fff); bad(p); /* ILT */
    fixture(p); w32(p + 0x410, 0x2fff); bad(p); /* IAT */
    fixture(p); w32(p + 0x440, 0x2fff); bad(p); /* hint/name */
    fixture(p); memset(p + 0x460, 'X', 0x200 - 0x60); bad(p); /* no DLL NUL */
    fixture(p); memset(p + 0x472, 'X', 0x200 - 0x72); bad(p); /* no symbol NUL */
    fixture(p); memset(p + 0x420, 0x7f, 20); bad(p); /* no terminating descriptor */
    fixture(p); w32(p + 0x80 + 24 + 108, 20); bad(p); /* no descriptor terminator */
    fixture(p); w32(p + 0x440, 0x80000001u);
    assert(cricket07_pe32_imports_preflight(p, sizeof p, &count) && count == 1);
    fixture(p); w32(p + 0x440, 0); bad(p); /* empty thunk list */
    return 0;
}
