/* Exercise the real iOS load_image() with a synthetic PE32 only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../runtime/runtime.h"

static void w16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
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
    p[0x200] = 0xab;
    w32(p + 0x400, 0x2040);
    w32(p + 0x40c, 0x2060);
    w32(p + 0x410, 0x2050);
    w32(p + 0x440, 0x2070);
    w32(p + 0x450, 0x2070);
    memcpy(p + 0x460, "Kernel32.dll", 13);
    memcpy(p + 0x472, "ExitProcess", 12);
}
static void write_fixture(const char *path, const uint8_t *bytes) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(bytes, 1, 4096, f) == 4096);
    assert(fclose(f) == 0);
}
int main(void) {
    uint8_t file[4096];
    fixture(file);
    g_mem = calloc(1, 8u * 1024u * 1024u);
    assert(g_mem);
    memset(g_mem + 0x400000, 0xa5, 0x4000);
    char path[] = "/tmp/cricket07-pe-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    LoadedImage out = {0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu};

    /* An invalid import directory fails without touching guest memory,
       registering imports or publishing image metadata. */
    w32(file + 0x40c, 0x2fffu);
    write_fixture(path, file);
    assert(load_image(path, &out) != 0);
    assert(out.image_base == 0xaaaaaaaau && out.entry == 0xbbbbbbbbu);
    assert(out.num_imports == 0xccccccccu);
    assert(g_image_lo == 0 && g_image_hi == 0);
    assert(runtime_import_slots_remaining() == 4096u);
    for (size_t i = 0; i < 0x4000; ++i) assert(g_mem[0x400000 + i] == 0xa5);

    fixture(file);
    write_fixture(path, file);
    assert(load_image(path, &out) == 0);
    assert(out.image_base == 0x400000u && out.entry == 0x401000u);
    assert(out.num_imports == 1u);
    assert(g_image_lo == 0x400000u && g_image_hi == 0x404000u);
    assert(g_mem[0x401000] == 0xab);
    assert(R32(0x402050) == IMPORT_BASE);
    assert(lookup_function(IMPORT_BASE));
    assert(!strcmp(import_name(IMPORT_BASE), "KERNEL32.DLL!ExitProcess"));
    assert(runtime_import_slots_remaining() == 4095u);

    /* A second load is rejected without overwriting the live image. */
    g_mem[0x401000] = 0x5a;
    assert(load_image(path, &out) != 0);
    assert(g_mem[0x401000] == 0x5a);
    assert(runtime_import_slots_remaining() == 4095u);
    unlink(path);
    free(g_mem);
    g_mem = NULL;
    return 0;
}
