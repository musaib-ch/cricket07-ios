#include "FrameQueue.h"
#include "TriangleStaging.h"
#include "VertexTransform.h"
#include "ViewportBridge.h"
#include "render_cmd.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Experimental frame consumer: multiple untextured triangle-list CMD_DRAW
 * commands, plus backbuffer TARGET/CLEAR/PRESENT. Not a complete D3D8 renderer.
 * A real game's transforms, depth, blend and texture state are not handled.
 */
static int take_triangle_frame_impl(float *rgba, size_t color_capacity,
                                    float *xyzw, size_t vertex_float_capacity,
                                    size_t *vertex_count, unsigned int *frame_number,
                                    int transform_positions,
                                    Cricket07DrawCall *calls, size_t call_capacity,
                                    size_t *call_count, uint32_t target_width,
                                    uint32_t target_height) {
    if (!rgba || color_capacity < 4 || !xyzw || !vertex_count || !frame_number) return 0;
    if (calls && (!call_count || !call_capacity || !target_width || !target_height)) return 0;
    const size_t limit = 16u * 1024u * 1024u;
    unsigned char *bytes = malloc(limit);
    if (!bytes) return 0;
    size_t length = cricket07_frame_queue_take(bytes, limit);
    if (length == 0) { free(bytes); return 0; }
    float color[4] = {0, 0, 0, 1};
    size_t vertices = 0, offset = 0;
    unsigned int frame = 0;
    int presented = 0, drawn = 0, cleared = 0, ok = 1;
    size_t n_calls = 0;
    uint32_t logical_width = 0, logical_height = 0;
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
            if (calls) {
                if (!t.lw || !t.lh || t.lw > 16384u || t.lh > 16384u) ok = 0;
                logical_width = t.lw;
                logical_height = t.lh;
            }
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
            if (transform_positions) {
                CmdDraw draw;
                memcpy(&draw, item, sizeof(draw)); /* extractor validated command length */
                if (!cricket07_transform_draw_positions(&draw, xyzw + vertices * 4u,
                                                        added, xyzw + vertices * 4u,
                                                        remaining)) {
                    ok = 0; break;
                }
            }
            if (calls && ok) {
                if (!logical_width || !logical_height || n_calls >= call_capacity ||
                    vertices > UINT32_MAX || added > UINT32_MAX - vertices) {
                    ok = 0; break;
                }
                CmdDraw draw;
                memcpy(&draw, item, sizeof(draw));
                Cricket07DrawCall call = {0};
                if (!cricket07_resolve_viewport(&draw, target_width, target_height,
                                                logical_width, logical_height,
                                                &call.viewport)) {
                    ok = 0; break;
                }
                call.first_vertex = (uint32_t)vertices;
                call.vertex_count = (uint32_t)added;
                calls[n_calls++] = call;
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
    if (calls) *call_count = n_calls;
    return 1;
}

/* Retain raw-vertex API for staging tests and debugging. */
int cricket07_take_triangle_frame(float *rgba, size_t color_capacity,
                                  float *xyzw, size_t vertex_float_capacity,
                                  size_t *vertex_count, unsigned int *frame_number) {
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 0, NULL, 0, NULL, 0, 0);
}

/* Metal consumer: reject unsupported transforms instead of drawing raw D3D
 * coordinates as if they were already Metal clip-space positions. */
int cricket07_take_transformed_triangle_frame(float *rgba, size_t color_capacity,
                                              float *xyzw, size_t vertex_float_capacity,
                                              size_t *vertex_count, unsigned int *frame_number) {
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, NULL, 0, NULL, 0, 0);
}


/* Preserve each draw's viewport and range for Metal encoder state changes. */
int cricket07_take_viewport_triangle_frame(float *rgba, size_t color_capacity,
                                          float *xyzw, size_t vertex_float_capacity,
                                          size_t *vertex_count, unsigned int *frame_number,
                                          Cricket07DrawCall *calls, size_t call_capacity,
                                          size_t *call_count, uint32_t target_width,
                                          uint32_t target_height) {
    if (!calls || !call_count) return 0;
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, calls, call_capacity,
                                    call_count, target_width, target_height);
}

/* Synthetic complete command stream, exercising the same mailbox and
 * CMD_DRAW parser that the iOS Metal consumer uses. This is NOT game data.
 * Keep this producer isolated from the actual engine until supported
 * render states, transforms, and textures are implemented.
 */
int cricket07_submit_test_triangle_frame(void) {
    const size_t draw_size = (sizeof(CmdDraw) + 3u * sizeof(CVert) + 7u) & ~(size_t)7u;
    unsigned char bytes[sizeof(CmdTarget) + sizeof(CmdClear) + sizeof(CmdDraw) + 3u * sizeof(CVert)
                        + 8u + sizeof(CmdPresent)] = {0};
    CmdTarget target = {0};
    target.h.type = CMD_TARGET;
    target.h.size = (uint32_t)sizeof(target);
    target.color_w = 640u; target.color_h = 480u;
    target.lw = 640u; target.lh = 480u;
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
    draw.vp[2] = 640.0f; draw.vp[3] = 480.0f;
    draw.maxz = 1.0f;
    /* Clip-space diagnostic geometry with the reference half-pixel correction. */
    draw.u.flags[0] = 2u;
    draw.u.vp[2] = 640.0f;
    draw.u.vp[3] = 480.0f;
    CVert vertices[3] = {0};
    vertices[0].x = -0.7f; vertices[0].y = -0.55f; vertices[0].w = 1.0f;
    vertices[1].x =  0.7f; vertices[1].y = -0.55f; vertices[1].w = 1.0f;
    vertices[2].x =  0.0f; vertices[2].y =  0.7f;  vertices[2].w = 1.0f;

    CmdPresent present = {0};
    present.h.type = CMD_PRESENT;
    present.h.size = (uint32_t)sizeof(present);
    present.frame = 7u;

    size_t offset = 0;
    memcpy(bytes + offset, &target, sizeof(target));
    offset += sizeof(target);
    memcpy(bytes + offset, &clear, sizeof(clear));
    offset += sizeof(clear);
    memcpy(bytes + offset, &draw, sizeof(draw));
    memcpy(bytes + offset + sizeof(draw), vertices, sizeof(vertices));
    offset += draw_size;
    memcpy(bytes + offset, &present, sizeof(present));
    offset += sizeof(present);
    return cricket07_frame_queue_publish(bytes, offset);
}
