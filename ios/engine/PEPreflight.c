#include "PEPreflight.h"
#include <string.h>
#define MAX_EXE_SIZE (128u * 1024u * 1024u)
#define MAX_SECTIONS 96u
#define GUEST_IMAGE_LO 0x00300000u
#define GUEST_IMAGE_HI 0x10000000u
static uint16_t read16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int range_ok(size_t offset, size_t count, size_t size) {
    return offset <= size && count <= size - offset;
}
static int power_of_two(uint32_t x) { return x && !(x & (x - 1u)); }
int cricket07_pe32_preflight(const uint8_t *bytes, size_t length,
                            Cricket07PEInfo *out) {
    if (!bytes || !out || length < 64 || length > MAX_EXE_SIZE) return 0;
    if (read16(bytes) != 0x5a4d) return 0;
    size_t peoff = read32(bytes + 0x3c);
    if (!range_ok(peoff, 24, length)) return 0;
    const uint8_t *pe = bytes + peoff;
    if (read32(pe) != 0x00004550u || read16(pe + 4) != 0x014cu) return 0;
    uint16_t nsec = read16(pe + 6), optsz = read16(pe + 20);
    if (!nsec || nsec > MAX_SECTIONS || optsz < 112) return 0;
    size_t optoff = peoff + 24;
    if (!range_ok(optoff, optsz, length)) return 0;
    const uint8_t *opt = bytes + optoff;
    if (read16(opt) != 0x010bu) return 0;
    uint32_t entry = read32(opt + 16), base = read32(opt + 28);
    uint32_t section_alignment = read32(opt + 32), file_alignment = read32(opt + 36);
    uint32_t image_size = read32(opt + 56), headers_size = read32(opt + 60);
    uint32_t directory_count = read32(opt + 92);
    uint32_t import_rva = read32(opt + 104), import_size = read32(opt + 108);
    if (!power_of_two(section_alignment) || !power_of_two(file_alignment) ||
        section_alignment < file_alignment || file_alignment < 512 ||
        file_alignment > 65536) return 0;
    if (base < GUEST_IMAGE_LO || base >= GUEST_IMAGE_HI ||
        !image_size || image_size > GUEST_IMAGE_HI - base) return 0;
    if (!headers_size || headers_size > length || headers_size > image_size) return 0;
    size_t sectoff = optoff + optsz, sectbytes = (size_t)nsec * 40u;
    if (!range_ok(sectoff, sectbytes, headers_size)) return 0;
    if (!entry || entry >= image_size) return 0;
    /* load_image() assumes an import directory and otherwise reads the DOS header. */
    if (directory_count < 2 || directory_count > (optsz - 96u) / 8u ||
        !import_rva || import_size < 20 ||
        import_rva >= image_size || import_size > image_size - import_rva) return 0;
    int entry_in_code = 0, import_in_section = 0;
    for (uint16_t i = 0; i < nsec; ++i) {
        const uint8_t *sec = bytes + sectoff + (size_t)i * 40u;
        uint32_t vsize = read32(sec + 8), va = read32(sec + 12);
        uint32_t rawsize = read32(sec + 16), rawptr = read32(sec + 20);
        uint32_t flags = read32(sec + 36);
        if (!vsize || va >= image_size || vsize > image_size - va) return 0;
        if (!range_ok(rawptr, rawsize, length) ||
            (rawsize && rawptr < headers_size)) return 0;
        if (entry >= va && entry - va < vsize && (flags & 0x20000000u))
            entry_in_code = 1;
        if (import_rva >= va && import_rva - va < vsize &&
            import_size <= vsize - (import_rva - va))
            import_in_section = 1;
        for (uint16_t j = 0; j < i; ++j) {
            const uint8_t *other = bytes + sectoff + (size_t)j * 40u;
            uint32_t other_va = read32(other + 12), other_size = read32(other + 8);
            if (va < other_va + other_size && other_va < va + vsize) return 0;
        }
    }
    if (!entry_in_code || !import_in_section) return 0;
    Cricket07PEInfo candidate = {base, image_size, entry, import_rva, nsec};
    memcpy(out, &candidate, sizeof candidate);
    return 1;
}
