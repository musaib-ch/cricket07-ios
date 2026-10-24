#include "FrameQueue.h"
#include "TriangleStaging.h"
#include "render_cmd.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Experimental frame consumer: one untextured triangle-list CMD_DRAW,
 * plus backbuffer TARGET/CLEAR/PRESENT. Not a complete D3D8 renderer.
 * A real game's transforms, depth, blend and texture state are not handled.
 */
int cricket07_take_triangle_frame(float *rgba, size_t color_capacity,
                                  float *xyzw, size_t vertex_float_capacity,
                                  size_t *vertex_count, unsigned int *frame_number) {
    if (!rgba || color_capacity < 4 || !xyzw || !vertex_count || !frame_number) return 0;
    const size_t limit = 16u * 1024u * 1024u;
    unsigned char *bytes = malloc(limit);
    if (!bytes) return 0;
    size_t length = cricket07_frame_queue_take(bytes, limit);
    if (length == 0) { free(bytes); return 0; }
    float color[4] = {0, 0, 0, 1};
    size_t vertices = 0, offset = 0;
    unsigned int frame = 0;
    int presented = 0, drawn = 0, cleared = 0, ok = 1;
    while (offset < length && ok) {
        CmdHdr h;
        if (length - offset < sizeof(h)) { ok = 0; break; }
        memcpy(&h, bytes + offset, sizeof(h));
        if (h.size < sizeof(h) || (h.size & 7u) || h.size > length - offset || presented) {
            ok = 0; break;
        }
        const unsigned char *item = bytes + offset;
        if (h.type == CMD_TARGET) {
            CmdTarget t;
            if (h.size < sizeof(t)) { ok = 0; break; }
            memcpy(&t, item, sizeof(t));
            if (t.color_id != 0) ok = 0;
        } else if (h.type == CMD_CLEAR) {
            CmdClear c;
            if (h.size < sizeof(c)) { ok = 0; break; }
            memcpy(&c, item, sizeof(c));
            if (!(c.flags & 1u)) { ok = 0; break; }
            color[0] = ((c.color >> 16) & 255) / 255.0f;
            color[1] = ((c.color >> 8) & 255) / 255.0f;
            color[2] = (c.color & 255) / 255.0f;
            color[3] = ((c.color >> 24) & 255) / 255.0f;
            cleared = 1;
        } else if (h.type == CMD_DRAW) {
            if (drawn || !cricket07_extract_triangles(item, h.size, xyzw,
                                                      vertex_float_capacity, &vertices)) {
                ok = 0; break;
            }
            drawn = 1;
        } else if (h.type == CMD_PRESENT) {
            CmdPresent c;
            if (h.size < sizeof(c)) { ok = 0; break; }
            memcpy(&c, item, sizeof(c));
            frame = c.frame;
            presented = 1;
        } else {
            ok = 0;
        }
        offset += h.size;
    }
    free(bytes);
    if (!ok || !drawn || !presented || !cleared || offset != length) return 0;
    memcpy(rgba, color, sizeof(color));
    *vertex_count = vertices;
    *frame_number = frame;
    return 1;
}
