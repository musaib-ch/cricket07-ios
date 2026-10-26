/* Synthetic PE32 fixture only. No game files. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/PEImportPlan.h"
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
    w32(p + 0x400, 0x2040);
    w32(p + 0x40c, 0x2060);
    w32(p + 0x410, 0x2050);
    w32(p + 0x440, 0x2070);
    w32(p + 0x450, 0x2070);
    memcpy(p + 0x460, "Kernel32.dll", 13);
    memcpy(p + 0x472, "ExitProcess", 12);
}
int main(void) {
    uint8_t file[4096];
    Cricket07PEImportPlan *plan = calloc(1, sizeof *plan);
    assert(plan);
    fixture(file);
    assert(cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 1);
    assert(plan->entries[0].iat_rva == 0x2050);
    assert(!strcmp(plan->entries[0].name, "KERNEL32.DLL!ExitProcess"));
    /* The staged name is independent of subsequent changes to the source. */
    memset(file + 0x472, 'x', 11);
    assert(!strcmp(plan->entries[0].name, "KERNEL32.DLL!ExitProcess"));
    /* OriginalFirstThunk == FirstThunk: both imports must be staged
       before the loader overwrites any thunk in guest memory. */
    fixture(file);
    w32(file + 0x410, 0x2040);
    w32(file + 0x444, 0x2080);
    memcpy(file + 0x482, "GetTickCount", 13);
    assert(cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 2);
    assert(plan->entries[0].iat_rva == 0x2040);
    assert(plan->entries[1].iat_rva == 0x2044);
    assert(!strcmp(plan->entries[1].name, "KERNEL32.DLL!GetTickCount"));
    memset(file + 0x440, 0xff, 8);
    assert(!strcmp(plan->entries[0].name, "KERNEL32.DLL!ExitProcess"));
    assert(!strcmp(plan->entries[1].name, "KERNEL32.DLL!GetTickCount"));
    fixture(file);
    w32(file + 0x440, 0x80000023u);
    assert(cricket07_pe32_stage_imports(file, sizeof file, 4095, plan));
    assert(!strcmp(plan->entries[0].name, "KERNEL32.DLL!#35"));
    assert(!cricket07_pe32_stage_imports(file, sizeof file, 4096, plan));
    assert(plan->count == 0);
    fixture(file);
    w32(file + 0x40c, 0x2fffu);
    assert(!cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 0);
    /* The runtime import-slot name is 96 bytes including NUL.
       A 95-character DLL!function name must fit exactly. */
    fixture(file);
    memset(file + 0x472, 'F', 82);
    file[0x472 + 82] = 0;
    assert(cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 1);
    assert(strlen(plan->entries[0].name) == 95);

    /* A one-byte overflow must fail without publishing a partial plan. */
    fixture(file);
    memset(file + 0x472, 'F', 83);
    file[0x472 + 83] = 0;
    assert(!cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 0);

    /* A long DLL name must not silently alias another import handler. */
    fixture(file);
    memset(file + 0x460, 'D', 96);
    file[0x460 + 96] = 0;
    assert(!cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    assert(plan->count == 0);

    assert(!cricket07_pe32_stage_imports(file, sizeof file, 0, NULL));
    free(plan);
    return 0;
}
