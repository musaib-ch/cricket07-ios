#include "TriangleStaging.h"
#include "render_cmd.h"
#include <stdint.h>
#include <string.h>

/* Diagnostic only: construct a CMD_DRAW using the same CVert wire layout
 * as the translated game and pass it through the real bounds-checked parser.
 * Output is clip-space XYZW, three vertices, no game-owned assets.
 */
int cricket07_sample_draw_vertices(float *xyzw, size_t capacity, size_t *count) {
    unsigned char bytes[sizeof(CmdDraw) + 3 * sizeof(CVert) + 8] = {0};
    CmdDraw cmd = {0};
    cmd.h.type = CMD_DRAW;
    cmd.h.size = (uint32_t)((sizeof(CmdDraw) + 3 * sizeof(CVert) + 7u) & ~7u);
    cmd.key.topo = 4;
    cmd.nverts = 3;
    CVert verts[3] = {0};
    verts[0].x = -0.7f; verts[0].y = -0.55f; verts[0].w = 1;
    verts[1].x =  0.7f; verts[1].y = -0.55f; verts[1].w = 1;
    verts[2].x =  0.0f; verts[2].y =  0.7f;  verts[2].w = 1;
    memcpy(bytes, &cmd, sizeof cmd);
    memcpy(bytes + sizeof cmd, verts, sizeof verts);
    return cricket07_extract_triangles(bytes, cmd.h.size, xyzw, capacity, count);
}
