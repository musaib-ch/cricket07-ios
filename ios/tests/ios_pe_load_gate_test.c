/* Synthetic PE32 only. No proprietary executable or assets. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/PELoadGate.h"
static void w16(uint8_t *p, uint16_t n) { p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8); }
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
    w32(p + 0x400, 0x2040);
    w32(p + 0x40c, 0x2060);
    w32(p + 0x410, 0x2050);
    w32(p + 0x440, 0x2070);
    w32(p + 0x450, 0x2070);
    memcpy(p + 0x460, "KERNEL32.dll", 13);
    memcpy(p + 0x472, "ExitProcess", 12);
}
int main(void) {
    uint8_t p[4096];
    Cricket07PEInfo out;
    fixture(p);
    memset(&out, 0xa5, sizeof out);
    assert(cricket07_pe32_load_gate(p, sizeof p, 0, &out));
    assert(out.image_base == 0x400000 && out.entry_rva == 0x1000);
    assert(cricket07_pe32_load_gate(p, sizeof p, 4095, &out));
    memset(&out, 0xa5, sizeof out);
    assert(!cricket07_pe32_load_gate(p, sizeof p, 4096, &out));
    assert(out.image_base == 0xa5a5a5a5u);
    assert(!cricket07_pe32_load_gate(p, sizeof p, 4097, &out));
    assert(!cricket07_pe32_load_gate(p, sizeof p, 0, NULL));
    assert(!cricket07_pe32_load_gate(NULL, sizeof p, 0, &out));
    fixture(p); w32(p + 0x40c, 0x2fff);
    assert(!cricket07_pe32_load_gate(p, sizeof p, 0, &out));
    fixture(p); w32(p + 0x80 + 24 + 28, 0xffff0000u);
    assert(!cricket07_pe32_load_gate(p, sizeof p, 0, &out));
    return 0;
}
