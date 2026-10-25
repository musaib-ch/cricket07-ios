#include "FrameQueue.h"
#include "TextureUploadBridge.h"
#include "TriangleStaging.h"
#include "VertexTransform.h"
#include "ViewportBridge.h"
#include "render_cmd.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Experimental bounded frame consumer: triangle-list CMD_DRAW plus
 * backbuffer TARGET/CLEAR/PRESENT. A narrow stage-0 texture modulation
 * subset is supported by the opt-in colored UV path. Not a D3D8 renderer.
 */
static int take_triangle_frame_impl(float *rgba, size_t color_capacity,
                                    float *xyzw, size_t vertex_float_capacity,
                                    size_t *vertex_count, unsigned int *frame_number,
                                    int transform_positions,
                                    Cricket07DrawCall *calls, size_t call_capacity,
                                    size_t *call_count, uint32_t target_width,
                                    uint32_t target_height,
                                    float *diffuse_rgba, size_t diffuse_float_capacity,
                                    float *uv0, size_t uv_float_capacity,
                                    void *texture_commands, size_t texture_capacity,
                                    size_t *texture_length) {
    if (!rgba || color_capacity < 4 || !xyzw || !vertex_count || !frame_number) return 0;
    if (calls && (!call_count || !call_capacity || !target_width || !target_height)) return 0;
    if (diffuse_rgba && diffuse_float_capacity < 4u) return 0;
    if (uv0 && (!diffuse_rgba || uv_float_capacity < 2u)) return 0;
    if (texture_commands && (!texture_length || !uv0 || !texture_capacity)) return 0;
    const size_t limit = 16u * 1024u * 1024u;
    unsigned char *bytes = malloc(limit);
    if (!bytes) return 0;
    size_t length = cricket07_frame_queue_take(bytes, limit);
    if (length == 0) { free(bytes); return 0; }
    float color[4] = {0, 0, 0, 1};
    size_t vertices = 0, offset = 0;
    unsigned int frame = 0;
    int presented = 0, drawn = 0, cleared = 0, ok = 1;
    size_t n_calls = 0, texture_written = 0, texture_ops = 0;
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
            /* Unlit diagnostic diffuse colors. CVert.diffuse is D3DCOLOR
             * 0xAARRGGBB. Do not misrepresent textured, lit or shader draws
             * as correctly shaded geometry in this narrow staging path. */
            uint32_t texture_id = 0;
            if (diffuse_rgba) {
                CmdDraw draw;
                memcpy(&draw, item, sizeof(draw)); /* validated by extractor */
                /* The UV-enabled Metal path may accept one narrowly supported
                 * fixed-function texture stage. Diffuse-only mode still
                 * rejects all texture-bound draws. */
                if (draw.pixel_shader || draw.u.flags[3] ||
                    (!uv0 && (draw.slot_kind[0] || draw.slot_kind[1] ||
                              draw.slot_kind[2] || draw.slot_kind[3])) ||
                    (uv0 && !cricket07_resolve_texture_binding(&draw, &texture_id)) ||
                    vertices > diffuse_float_capacity / 4u ||
                    added > diffuse_float_capacity / 4u - vertices) {
                    ok = 0; break;
                }
                if (uv0 && (vertices > uv_float_capacity / 2u ||
                            added > uv_float_capacity / 2u - vertices)) {
                    ok = 0; break;
                }
                const unsigned char *source = item + sizeof(CmdDraw);
                for (size_t i = 0; i < added; ++i) {
                    CVert vertex;
                    memcpy(&vertex, source + i * sizeof(CVert), sizeof(vertex));
                    uint32_t packed = vertex.diffuse;
                    float *out = diffuse_rgba + (vertices + i) * 4u;
                    out[0] = (float)((packed >> 16) & 255u) / 255.0f;
                    out[1] = (float)((packed >> 8) & 255u) / 255.0f;
                    out[2] = (float)(packed & 255u) / 255.0f;
                    out[3] = (float)((packed >> 24) & 255u) / 255.0f;
                    if (uv0) {
                        const float u = vertex.tc[0][0], v = vertex.tc[0][1];
                        if (!isfinite(u) || !isfinite(v)) { ok = 0; break; }
                        uv0[(vertices + i) * 2u] = u;
                        uv0[(vertices + i) * 2u + 1u] = v;
                    }
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
                if (!cricket07_resolve_draw_state(&draw, &call.state)) {
                    ok = 0; break;
                }
                call.first_vertex = (uint32_t)vertices;
                call.vertex_count = (uint32_t)added;
                call.texture_id = texture_id;
                calls[n_calls++] = call;
            }
            vertices += added;
            drawn = 1;
        } else if (h.type == CMD_UPLOAD_TEX || h.type == CMD_DESTROY_TEX) {
            /* Texture mutations must precede all draws in this narrow path.
             * Preserve complete validated commands for Metal upload after
             * the frame is fully validated, never a pointer into freed bytes. */
            if (!texture_commands || drawn || texture_ops >= 256u ||
                texture_written > texture_capacity ||
                h.size > texture_capacity - texture_written) {
                ok = 0; break;
            }
            if (h.type == CMD_UPLOAD_TEX) {
                Cricket07TextureUpload decoded;
                if (!cricket07_decode_texture_upload(item, h.size, &decoded)) {
                    ok = 0; break;
                }
            } else {
                CmdDestroy destroy;
                if (h.size != ((sizeof(destroy) + 7u) & ~(size_t)7u)) {
                    ok = 0; break;
                }
                memcpy(&destroy, item, sizeof(destroy));
                if (!destroy.id) { ok = 0; break; }
            }
            memcpy((unsigned char *)texture_commands + texture_written, item, h.size);
            texture_written += h.size;
            ++texture_ops;
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
    if (texture_commands) *texture_length = texture_written;
    return 1;
}

/* Retain raw-vertex API for staging tests and debugging. */
int cricket07_take_triangle_frame(float *rgba, size_t color_capacity,
                                  float *xyzw, size_t vertex_float_capacity,
                                  size_t *vertex_count, unsigned int *frame_number) {
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 0, NULL, 0, NULL, 0, 0, NULL, 0, NULL, 0, NULL, 0, NULL);
}

/* Metal consumer: reject unsupported transforms instead of drawing raw D3D
 * coordinates as if they were already Metal clip-space positions. */
int cricket07_take_transformed_triangle_frame(float *rgba, size_t color_capacity,
                                              float *xyzw, size_t vertex_float_capacity,
                                              size_t *vertex_count, unsigned int *frame_number) {
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, NULL, 0, NULL, 0, 0, NULL, 0, NULL, 0, NULL, 0, NULL);
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
                                    call_count, target_width, target_height, NULL, 0, NULL, 0, NULL, 0, NULL);
}

/* New opt-in diagnostic path: preserves per-vertex D3DCOLOR diffuse
 * alongside transformed positions and per-draw state. This is NOT a
 * fixed-function texture-stage or pixel-shader implementation. */
int cricket07_take_colored_viewport_triangle_frame(
    float *rgba, size_t color_capacity,
    float *xyzw, size_t vertex_float_capacity,
    float *diffuse_rgba, size_t diffuse_float_capacity,
    size_t *vertex_count, unsigned int *frame_number,
    Cricket07DrawCall *calls, size_t call_capacity, size_t *call_count,
    uint32_t target_width, uint32_t target_height) {
    if (!calls || !call_count || !diffuse_rgba) return 0;
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, calls, call_capacity,
                                    call_count, target_width, target_height,
                                    diffuse_rgba, diffuse_float_capacity, NULL, 0);
}

/* Optional UV0 path. Read CVert.tc[0].xy verbatim from the recorded draw,
 * without inferring texture stage bindings or enabling game texture draws.
 * Existing untextured APIs remain unchanged. */
int cricket07_take_colored_uv_viewport_triangle_frame(
    float *rgba, size_t color_capacity,
    float *xyzw, size_t vertex_float_capacity,
    float *diffuse_rgba, size_t diffuse_float_capacity,
    float *uv0, size_t uv_float_capacity,
    size_t *vertex_count, unsigned int *frame_number,
    Cricket07DrawCall *calls, size_t call_capacity, size_t *call_count,
    uint32_t target_width, uint32_t target_height) {
    if (!calls || !call_count || !diffuse_rgba || !uv0) return 0;
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, calls, call_capacity,
                                    call_count, target_width, target_height,
                                    diffuse_rgba, diffuse_float_capacity,
                                    uv0, uv_float_capacity, NULL, 0, NULL);
}

/* Opt-in texture-operation path. Only uploads/destroys before the first
 * draw are supported, and the caller must apply the returned commands
 * before submitting the validated draw calls to Metal. */
int cricket07_take_colored_uv_texture_frame(
    float *rgba, size_t color_capacity,
    float *xyzw, size_t vertex_float_capacity,
    float *diffuse_rgba, size_t diffuse_float_capacity,
    float *uv0, size_t uv_float_capacity,
    size_t *vertex_count, unsigned int *frame_number,
    Cricket07DrawCall *calls, size_t call_capacity, size_t *call_count,
    uint32_t target_width, uint32_t target_height,
    void *texture_commands, size_t texture_capacity, size_t *texture_length) {
    if (!calls || !call_count || !diffuse_rgba || !uv0 ||
        !texture_commands || !texture_length) return 0;
    return take_triangle_frame_impl(rgba, color_capacity, xyzw, vertex_float_capacity,
                                    vertex_count, frame_number, 1, calls, call_capacity,
                                    call_count, target_width, target_height,
                                    diffuse_rgba, diffuse_float_capacity,
                                    uv0, uv_float_capacity,
                                    texture_commands, texture_capacity, texture_length);
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
    draw.key.cmask = 15u; /* write all RGBA channels */
    /* Exercise the same validated texture binding used by the Metal
     * consumer. Only generated diagnostic texture ID 1 is resident. */
    draw.slot_kind[0] = TS_TEX;
    draw.slot_id[0] = 1u;
    draw.samp_key[0] = 432u; /* nearest, clamp U/V, no mip filtering */
    draw.u.st0[0] = 4u; draw.u.st0[1] = 2u;
    draw.u.st0[2] = 0u; draw.u.st0[3] = 4u;
    draw.u.st0b[0] = 2u; draw.u.st0b[1] = 0u;
    draw.u.st1[0] = 1u; /* stage 1 disabled */
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
    vertices[0].diffuse = 0xFFFF3333u;
    vertices[1].diffuse = 0xFF33CC66u;
    vertices[2].diffuse = 0xFF4D80FFu;
    /* Distinct UVs, not derived from clip-space positions. */
    vertices[0].tc[0][0] = 0.0f; vertices[0].tc[0][1] = 1.0f;
    vertices[1].tc[0][0] = 1.0f; vertices[1].tc[0][1] = 1.0f;
    vertices[2].tc[0][0] = 0.5f; vertices[2].tc[0][1] = 0.0f;

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
