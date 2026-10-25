#include "FrameQueue.h"
#include "TriangleStaging.h"
#include "render_cmd.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Experimental frame consumer: multiple untextured triangle-list CMD_DRAW
 * commands, plus backbuffer TARGET/CLEAR/PRESENT. Not a complete D3D8 renderer.
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
            if (!cleared || vertices > vertex_float_capacity / 4u) {
                ok = 0; break;
            }
            size_t added = 0;
            size_t remaining = vertex_float_capacity - vertices * 4u;
            if (!cricket07_extract_triangles(item, h.size, xyzw + vertices * 4u,
                                             remaining, &added)) {
                ok = 0; break;
            }
            vertices += added;
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


/* Synthetic complete command stream, exercising the same mailbox and
 * CMD_DRAW parser that the iOS Metal consumer uses. This is NOT game data.
 * Keep this producer isolated from the actual engine until supported
 * render states, transforms, and textures are implemented.
 */
int cricket07_submit_test_triangle_frame(void) {
    const size_t draw_size = (sizeof(CmdDraw) + 3u * sizeof(CVert) + 7u) & ~(size_t)7u;
    unsigned char bytes[sizeof(CmdClear) + sizeof(CmdDraw) + 3u * sizeof(CVert)
                        + 8u + sizeof(CmdPresent)] = {0};
    CmdClear clear = {0};
    clear.h.type = CMD_CLEAR;
    clear.h.size = (uint32_t)sizeof(clear);
    clear.flags = 1u;
    clear.color = 0xFF166078u;

    CmdDraw draw = {0};
    draw.h.type = CMD_DRAW;
    draw.h.size = (uint32_t)draw_size;
    draw.key.topo = 4u;  /* WGPU triangle-list topology */
    draw.nverts = 3u;
    CVert vertices[3] = {0};
    vertices[0].x = -0.7f; vertices[0].y = -0.55f; vertices[0].w = 1.0f;
    vertices[1].x =  0.7f; vertices[1].y = -0.55f; vertices[1].w = 1.0f;
    vertices[2].x =  0.0f; vertices[2].y =  0.7f;  vertices[2].w = 1.0f;

    CmdPresent present = {0};
    present.h.type = CMD_PRESENT;
    present.h.size = (uint32_t)sizeof(present);
    present.frame = 7u;

    size_t offset = 0;
    memcpy(bytes + offset, &clear, sizeof(clear));
    offset += sizeof(clear);
    memcpy(bytes + offset, &draw, sizeof(draw));
    memcpy(bytes + offset + sizeof(draw), vertices, sizeof(vertices));
    offset += draw_size;
    memcpy(bytes + offset, &present, sizeof(present));
    offset += sizeof(present);
    return cricket07_frame_queue_publish(bytes, offset);
}
