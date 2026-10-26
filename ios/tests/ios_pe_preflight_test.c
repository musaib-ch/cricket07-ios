/* Synthetic PE32 only, no proprietary game files. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../engine/PEPreflight.h"
static void wr16(uint8_t *p, uint16_t n) { p[0] = n & 255; p[1] = n >> 8; }
static void wr32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i));
}
static void make_pe(uint8_t *p) {
    memset(p, 0, 4096);
    wr16(p, 0x5a4d); wr32(p + 0x3c, 0x80);
    uint8_t *pe = p + 0x80;
    wr32(pe, 0x4550); wr16(pe + 4, 0x14c);
    wr16(pe + 6, 2); wr16(pe + 20, 224);
    uint8_t *opt = pe + 24;
    wr16(opt, 0x10b); wr32(opt + 16, 0x1000);
    wr32(opt + 28, 0x400000); wr32(opt + 32, 0x1000);
    wr32(opt + 36, 0x200); wr32(opt + 56, 0x4000);
    wr32(opt + 60, 0x200); wr32(opt + 92, 16);
    wr32(opt + 104, 0x2000); wr32(opt + 108, 0x28);
    uint8_t *sec = opt + 224;
    wr32(sec + 8, 0x1000); wr32(sec + 12, 0x1000);
    wr32(sec + 16, 0x200); wr32(sec + 20, 0x200);
    wr32(sec + 36, 0x60000020);
    sec += 40;
    wr32(sec + 8, 0x1000); wr32(sec + 12, 0x2000);
    wr32(sec + 16, 0x200); wr32(sec + 20, 0x400);
    wr32(sec + 36, 0xc0000040);
}
static void rejected(const uint8_t *p, size_t size) {
    Cricket07PEInfo info;
    memset(&info, 0xa5, sizeof info);
    Cricket07PEInfo before = info;
    assert(!cricket07_pe32_preflight(p, size, &info));
    assert(memcmp(&before, &info, sizeof info) == 0);
}
int main(void) {
    uint8_t p[4096]; Cricket07PEInfo info;
    make_pe(p);
    assert(cricket07_pe32_preflight(p, sizeof p, &info));
    assert(info.image_base == 0x400000 && info.image_size == 0x4000);
    assert(info.entry_rva == 0x1000 && info.import_rva == 0x2000);
    assert(info.section_count == 2);
    rejected(NULL, sizeof p); rejected(p, 0); rejected(p, 63);
    rejected(p, 0x180); rejected(p, 0x3ff);
    rejected(p, (size_t)128 * 1024 * 1024 + 1);
#define BAD(off, val) do { make_pe(p); wr32(p + (off), (val)); rejected(p, sizeof p); } while (0)
    BAD(0x3c, 0xfffffff0u);
    BAD(0x80, 0xdeadbeefu);
    BAD(0x80 + 24 + 16, 0x3500);
    BAD(0x80 + 24 + 28, 0xffffff00u);
    BAD(0x80 + 24 + 56, 0x1000);
    BAD(0x80 + 24 + 60, 0x100);
    BAD(0x80 + 24 + 92, 1);
    BAD(0x80 + 24 + 92, 1000);
    BAD(0x80 + 24 + 104, 0);
    BAD(0x80 + 24 + 108, 0xfffffff0u);
    BAD(0x80 + 24 + 224 + 20, 0xffffff00u);
    BAD(0x80 + 24 + 224 + 40 + 12, 0x1800);
#undef BAD
    make_pe(p); wr32(p + 0x80 + 24 + 224 + 16, 0);
    wr32(p + 0x80 + 24 + 224 + 20, 0xffffff00u); rejected(p, sizeof p);
    make_pe(p); p[0] = 0; rejected(p, sizeof p);
    make_pe(p); wr16(p + 0x80 + 4, 0x8664); rejected(p, sizeof p);
    make_pe(p); wr16(p + 0x80 + 6, 97); rejected(p, sizeof p);
    make_pe(p); wr16(p + 0x80 + 20, 100); rejected(p, sizeof p);
    make_pe(p); wr16(p + 0x80 + 24, 0x20b); rejected(p, sizeof p);
    make_pe(p); wr32(p + 0x80 + 24 + 16, 0x2000); rejected(p, sizeof p);
    make_pe(p); wr32(p + 0x80 + 24 + 32, 3); rejected(p, sizeof p);
    make_pe(p); wr32(p + 0x80 + 24 + 36, 0); rejected(p, sizeof p);
    make_pe(p); wr32(p + 0x80 + 24 + 224 + 36, 0); rejected(p, sizeof p);
    return 0;
}
