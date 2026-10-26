#include "PEImageStage.h"
#include <stdlib.h>
#include <string.h>

#define MAX_IMPORT_SLOTS 4096u

static uint16_t read16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
    p[2] = (uint8_t)(n >> 16);
    p[3] = (uint8_t)(n >> 24);
}

int cricket07_pe32_stage_image(const uint8_t *file, size_t length,
                              const Cricket07PEInfo *info,
                              const Cricket07PEImportPlan *imports,
                              uint32_t occupied_slots, uint32_t import_base,
                              uint8_t **image) {
    if (!file || !info || !imports || !image ||
        occupied_slots > MAX_IMPORT_SLOTS || !imports->count ||
        imports->count > MAX_IMPORT_SLOTS - occupied_slots) return 0;

    /* Recheck the image metadata rather than trusting caller-supplied sizes.
       The import plan is separately validated by the caller's load gate. */
    Cricket07PEInfo checked;
    if (!cricket07_pe32_preflight(file, length, &checked) ||
        checked.image_base != info->image_base ||
        checked.image_size != info->image_size ||
        checked.entry_rva != info->entry_rva ||
        checked.import_rva != info->import_rva ||
        checked.section_count != info->section_count) return 0;

    /* All patch addresses and names must be valid before allocating or
       modifying the staging buffer. Reject overlapping IAT destinations. */
    for (uint32_t i = 0; i < imports->count; ++i) {
        const Cricket07PEStagedImport *e = &imports->entries[i];
        if (e->iat_rva > checked.image_size - 4u ||
            !memchr(e->name, '\0', sizeof e->name) || !e->name[0] ||
            import_base > UINT32_MAX - (occupied_slots + i) * 16u)
            return 0;
        for (uint32_t j = 0; j < i; ++j)
            if (e->iat_rva < imports->entries[j].iat_rva + 4u &&
                imports->entries[j].iat_rva < e->iat_rva + 4u) return 0;
    }

    uint8_t *staged = calloc(1, checked.image_size);
    if (!staged) return 0;
    size_t peoff = read32(file + 0x3c);
    const uint8_t *pe = file + peoff;
    size_t optoff = peoff + 24u;
    const uint8_t *opt = file + optoff;
    uint16_t optsz = read16(pe + 20);
    size_t sectoff = optoff + optsz;
    uint32_t headers = read32(opt + 60);
    memcpy(staged, file, headers);
    for (uint16_t i = 0; i < checked.section_count; ++i) {
        const uint8_t *sec = file + sectoff + (size_t)i * 40u;
        uint32_t vsize = read32(sec + 8), va = read32(sec + 12);
        uint32_t rawsize = read32(sec + 16), rawptr = read32(sec + 20);
        uint32_t n = rawsize < vsize ? rawsize : vsize;
        /* The PE preflight guarantees these file and virtual spans. */
        memset(staged + va, 0, vsize);
        if (n) memcpy(staged + va, file + rawptr, n);
    }
    for (uint32_t i = 0; i < imports->count; ++i) {
        uint32_t magic = import_base + (occupied_slots + i) * 16u;
        write32(staged + imports->entries[i].iat_rva, magic);
    }
    *image = staged;
    return 1;
}
