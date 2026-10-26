/* Synthetic PE32 image only. No proprietary game files. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../engine/PEImageStage.h"
#include "../engine/PELoadGate.h"

static void w16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static void w32(uint8_t *p, uint32_t n) {
    for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (i * 8));
}
static uint32_t r32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
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
    p[0x200] = 0xab; /* .text */
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
    fixture(file);
    Cricket07PEInfo info;
    Cricket07PEImportPlan *plan = calloc(1, sizeof *plan);
    assert(plan);
    assert(cricket07_pe32_load_gate(file, sizeof file, 0, &info));
    assert(cricket07_pe32_stage_imports(file, sizeof file, 0, plan));
    uint8_t *image = NULL;
    assert(cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                     0, 0xf0000000u, &image));
    assert(image);
    assert(image[0x1000] == 0xab);
    assert(image[0x1200] == 0); /* zero-filled virtual tail */
    assert(r32(image + 0x2050) == 0xf0000000u);
    assert(r32(file + 0x450) == 0x2070u); /* immutable source */
    free(image);

    /* Failure must not overwrite caller's output pointer or any file byte. */
    uint8_t snapshot[4096];
    memcpy(snapshot, file, sizeof snapshot);
    image = (uint8_t *)(uintptr_t)0x1234u;
    plan->entries[0].iat_rva = info.image_size - 3;
    assert(!cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                      0, 0xf0000000u, &image));
    assert(image == (uint8_t *)(uintptr_t)0x1234u);
    assert(!memcmp(file, snapshot, sizeof file));
    plan->entries[0].iat_rva = 0x2050;
    assert(!cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                      4096, 0xf0000000u, &image));
    assert(image == (uint8_t *)(uintptr_t)0x1234u);
    Cricket07PEInfo wrong = info;
    wrong.image_size = 0x8000;
    assert(!cricket07_pe32_stage_image(file, sizeof file, &wrong, plan,
                                      0, 0xf0000000u, &image));
    assert(image == (uint8_t *)(uintptr_t)0x1234u);

    /* Duplicate and partially overlapping IAT writes are rejected. */
    plan->entries[1] = plan->entries[0];
    plan->count = 2;
    assert(!cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                      0, 0xf0000000u, &image));
    plan->entries[1].iat_rva = plan->entries[0].iat_rva + 1u;
    assert(!cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                      0, 0xf0000000u, &image));
    plan->count = 1;

    /* The import address calculation must never wrap uint32_t. */
    assert(!cricket07_pe32_stage_image(file, sizeof file, &info, plan,
                                      1, UINT32_MAX - 1u, &image));
    assert(image == (uint8_t *)(uintptr_t)0x1234u);
    free(plan);
    return 0;
}
