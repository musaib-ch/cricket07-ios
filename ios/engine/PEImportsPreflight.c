#include "PEImportsPreflight.h"
#include "PEPreflight.h"
#include <string.h>

#define MAX_IMPORTS 4096u
#define MAX_NAME 256u
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int span(size_t at, size_t n, size_t length) {
    return at <= length && n <= length - at;
}
/* A loader-readable RVA must exist in raw file bytes, not merely in a
 * virtual zero-fill tail. Restrict each access to a single section. */
static int map_rva(const uint8_t *file, size_t length, size_t sections,
                   uint16_t nsec, uint32_t headers, uint32_t rva,
                   size_t n, size_t *offset) {
    if (!n) return 0;
    if (rva < headers && n <= headers - rva && span(rva, n, length)) {
        *offset = rva;
        return 1;
    }
    for (uint16_t i = 0; i < nsec; ++i) {
        const uint8_t *s = file + sections + (size_t)i * 40;
        uint32_t va = rd32(s + 12), virtual_size = rd32(s + 8);
        uint32_t raw_size = rd32(s + 16), raw_offset = rd32(s + 20);
        if (rva < va) continue;
        uint32_t delta = rva - va;
        if (delta >= virtual_size || delta >= raw_size ||
            n > virtual_size - delta || n > raw_size - delta) continue;
        size_t pos = (size_t)raw_offset + delta;
        if (!span(pos, n, length)) return 0;
        *offset = pos;
        return 1;
    }
    return 0;
}
static int rva_string(const uint8_t *file, size_t length, size_t sections,
                      uint16_t nsec, uint32_t headers, uint32_t rva) {
    size_t pos;
    for (size_t n = 0; n < MAX_NAME; ++n) {
        if (rva > UINT32_MAX - (uint32_t)n ||
            !map_rva(file, length, sections, nsec, headers,
                     rva + (uint32_t)n, 1, &pos)) return 0;
        if (!file[pos]) return n != 0;
    }
    return 0;
}
int cricket07_pe32_imports_preflight(const uint8_t *bytes, size_t length,
                                    uint32_t *import_count) {
    Cricket07PEInfo info;
    if (!import_count || !cricket07_pe32_preflight(bytes, length, &info)) return 0;
    size_t peoff = rd32(bytes + 0x3c), optoff = peoff + 24;
    const uint8_t *opt = bytes + optoff;
    uint32_t headers = rd32(opt + 60), import_size = rd32(opt + 108);
    size_t sections = optoff + rd16(bytes + peoff + 20);
    if (import_size < 40 || import_size % 20) return 0;
    size_t dir;
    if (!map_rva(bytes, length, sections, info.section_count, headers,
                 info.import_rva, import_size, &dir)) return 0;
    uint32_t count = 0;
    int terminated = 0;
    for (uint32_t i = 0; i < import_size / 20; ++i) {
        const uint8_t *d = bytes + dir + (size_t)i * 20;
        static const uint8_t zeros[20] = {0};
        if (!memcmp(d, zeros, sizeof zeros)) { terminated = 1; break; }
        uint32_t ilt = rd32(d), dll = rd32(d + 12), iat = rd32(d + 16);
        if (!dll || !iat ||
            !rva_string(bytes, length, sections, info.section_count, headers, dll)) return 0;
        if (!ilt) ilt = iat;
        int thunk_terminated = 0;
        for (uint32_t k = 0; k <= MAX_IMPORTS - count; ++k) {
            if (k > (UINT32_MAX - ilt) / 4 || k > (UINT32_MAX - iat) / 4) return 0;
            size_t src, dst;
            if (!map_rva(bytes, length, sections, info.section_count, headers,
                         ilt + k * 4, 4, &src) ||
                !map_rva(bytes, length, sections, info.section_count, headers,
                         iat + k * 4, 4, &dst)) return 0;
            uint32_t thunk = rd32(bytes + src);
            if (!thunk) { thunk_terminated = 1; break; }
            if (count == MAX_IMPORTS) return 0;
            if (!(thunk & 0x80000000u)) {
                size_t hint;
                if (thunk > UINT32_MAX - 2 ||
                    !map_rva(bytes, length, sections, info.section_count, headers,
                             thunk, 2, &hint) ||
                    !rva_string(bytes, length, sections, info.section_count,
                                headers, thunk + 2)) return 0;
            }
            ++count;
        }
        if (!thunk_terminated) return 0;
    }
    if (!terminated || !count) return 0;
    *import_count = count;
    return 1;
}
