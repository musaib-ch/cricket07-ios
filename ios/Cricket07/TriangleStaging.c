#include "render_cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Bounded extraction of untextured triangle vertices from a single CMD_DRAW.
 * This is a diagnostic staging API, NOT a replacement for D3D shader state.
 * Coordinates are copied in raw game-renderer space for later viewport conversion.
 */
int cricket07_extract_triangles(const void *data, size_t length,
                                float *xyzw, size_t float_capacity,
                                size_t *out_vertices) {
    if (!data || !xyzw || !out_vertices || length < sizeof(CmdDraw)) return 0;
    CmdDraw draw;
    memcpy(&draw, data, sizeof(draw));
    if (draw.h.type != CMD_DRAW || draw.h.size > length ||
        draw.h.size < sizeof(CmdDraw) || (draw.h.size & 7u) ||
        draw.key.topo != 4 || draw.nverts == 0 || draw.nverts % 3 != 0 ||
        draw.nverts > 65535 || draw.nverts > float_capacity / 4) return 0;
    size_t bytes = (size_t)draw.nverts * sizeof(CVert);
    if (bytes > (size_t)draw.h.size - sizeof(CmdDraw)) return 0;
    const unsigned char *verts = (const unsigned char *)data + sizeof(CmdDraw);
    for (size_t i = 0; i < draw.nverts; ++i) {
        CVert v;
        memcpy(&v, verts + i * sizeof(CVert), sizeof(v));
        if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z) || !isfinite(v.w)) return 0;
        xyzw[i * 4] = v.x;
        xyzw[i * 4 + 1] = v.y;
        xyzw[i * 4 + 2] = v.z;
        xyzw[i * 4 + 3] = v.w;
    }
    *out_vertices = draw.nverts;
    return 1;
}
