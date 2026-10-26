#include "PEImportPlan.h"
#include "PELoadGate.h"
#include <stdio.h>
#include <string.h>

#define MAX_NAME_BYTES 256u
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int span(size_t at, size_t n, size_t size) {
    return at <= size && n <= size - at;
}
static int map_rva(const uint8_t *file, size_t length, size_t sect,
                   uint16_t nsec, uint32_t headers, uint32_t rva,
                   size_t n, size_t *offset) {
    if (!n) return 0;
    if (rva < headers && n <= headers - rva && span(rva, n, length)) {
        *offset = rva;
        return 1;
    }
    for (uint16_t i = 0; i < nsec; ++i) {
        const uint8_t *s = file + sect + (size_t)i * 40u;
        uint32_t va = rd32(s + 12), vsz = rd32(s + 8);
        uint32_t raw = rd32(s + 20), rsz = rd32(s + 16);
        if (rva < va) continue;
        uint32_t delta = rva - va;
        if (delta >= vsz || delta >= rsz ||
            n > vsz - delta || n > rsz - delta) continue;
        size_t pos = (size_t)raw + delta;
        if (!span(pos, n, length)) return 0;
        *offset = pos;
        return 1;
    }
    return 0;
}
static int read_name(const uint8_t *file, size_t length, size_t sect,
                     uint16_t nsec, uint32_t headers, uint32_t rva,
                     char out[MAX_NAME_BYTES]) {
    for (uint32_t n = 0; n < MAX_NAME_BYTES; ++n) {
        size_t at;
        if (rva > UINT32_MAX - n ||
            !map_rva(file, length, sect, nsec, headers, rva + n, 1, &at))
            return 0;
        out[n] = (char)file[at];
        if (!out[n]) return n != 0;
    }
    return 0;
}
int cricket07_pe32_stage_imports(const uint8_t *file, size_t length,
                                uint32_t occupied_slots,
                                Cricket07PEImportPlan *plan) {
    if (!plan) return 0;
    plan->count = 0;
    Cricket07PEInfo info;
    if (!cricket07_pe32_load_gate(file, length, occupied_slots, &info))
        return 0;
    size_t peoff = rd32(file + 0x3c), optoff = peoff + 24;
    const uint8_t *opt = file + optoff;
    uint32_t headers = rd32(opt + 60), dir_size = rd32(opt + 108);
    size_t sections = optoff + rd16(file + peoff + 20);
    size_t dir;
    if (!map_rva(file, length, sections, info.section_count, headers,
                 info.import_rva, dir_size, &dir)) return 0;
    uint32_t count = 0;
    int terminated = 0;
    for (uint32_t i = 0; i < dir_size / 20u; ++i) {
        const uint8_t *d = file + dir + (size_t)i * 20u;
        static const uint8_t zero[20] = {0};
        if (!memcmp(d, zero, 20)) { terminated = 1; break; }
        uint32_t ilt = rd32(d), dll_rva = rd32(d + 12), iat = rd32(d + 16);
        char dll[MAX_NAME_BYTES];
        if (!read_name(file, length, sections, info.section_count,
                       headers, dll_rva, dll)) return 0;
        for (size_t j = 0; dll[j]; ++j)
            if (dll[j] >= 'a' && dll[j] <= 'z')
                dll[j] = (char)(dll[j] - 'a' + 'A');
        if (!ilt) ilt = iat;
        for (uint32_t k = 0; ; ++k) {
            size_t at;
            if (k > (UINT32_MAX - ilt) / 4u ||
                k > (UINT32_MAX - iat) / 4u ||
                !map_rva(file, length, sections, info.section_count, headers,
                         ilt + 4u * k, 4, &at)) return 0;
            uint32_t thunk = rd32(file + at);
            if (!thunk) break;
            if (count >= CRICKET07_PE_MAX_STAGED_IMPORTS - occupied_slots)
                return 0;
            Cricket07PEStagedImport *e = &plan->entries[count];
            e->iat_rva = iat + 4u * k;
            int written;
            if (thunk & 0x80000000u) {
                written = snprintf(e->name, sizeof e->name, "%s!#%u",
                                   dll, thunk & 0xffffu);
            } else {
                char fn[MAX_NAME_BYTES];
                if (thunk > UINT32_MAX - 2u ||
                    !read_name(file, length, sections, info.section_count,
                               headers, thunk + 2u, fn)) return 0;
                written = snprintf(e->name, sizeof e->name, "%s!%s", dll, fn);
            }
            /* The runtime dispatch table also uses a 96-byte name.
               Reject truncation rather than resolving a different symbol. */
            if (written < 0 || (size_t)written >= sizeof e->name) return 0;
            ++count;
        }
    }
    if (!terminated || !count) return 0;
    plan->count = count;
    return 1;
}
