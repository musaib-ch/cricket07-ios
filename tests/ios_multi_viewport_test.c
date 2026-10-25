#include "RuntimeBridge.h"
#include "FrameQueue.h"
#include "render_cmd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const size_t stride = (sizeof(CmdDraw) + 3u * sizeof(CVert) + 7u) & ~(size_t)7u;
    unsigned char stream[sizeof(CmdTarget) + sizeof(CmdClear) +
                         2u * (sizeof(CmdDraw) + 3u * sizeof(CVert) + 8u) +
                         sizeof(CmdPresent)] = {0};
    size_t offset = 0;
    CmdTarget target = {0};
    target.h.type = CMD_TARGET;
    target.h.size = sizeof(target);
    target.lw = 640; target.lh = 480;
    target.color_w = 640; target.color_h = 480;
    memcpy(stream + offset, &target, sizeof target); offset += sizeof target;
    CmdClear clear = {0};
    clear.h.type = CMD_CLEAR; clear.h.size = sizeof(clear);
    clear.flags = 1; clear.color = 0xff123456u;
    memcpy(stream + offset, &clear, sizeof clear); offset += sizeof clear;
    for (unsigned i = 0; i < 2; ++i) {
        CmdDraw draw = {0};
        draw.h.type = CMD_DRAW; draw.h.size = (uint32_t)stride;
        draw.key.topo = 4; draw.nverts = 3;
        draw.key.zen = i ? 1u : 0u;
        draw.key.zwrite = i ? 1u : 0u;
        draw.key.zfunc = i ? 5u : 0u;
        draw.key.cull = i ? 3u : 2u;
        draw.key.cmask = i ? 0xAu : 0xFu;
        draw.key.blend = i ? 1u : 0u;
        draw.key.src = i ? 5u : 0u;
        draw.key.dst = i ? 6u : 0u;
        draw.key.op = i ? 1u : 0u;
        draw.u.flags[0] = 2;
        draw.u.vp[2] = 640; draw.u.vp[3] = 480;
        draw.maxz = 1;
        draw.vp[0] = i ? 320.f : 0.f;
        draw.vp[1] = 0.f;
        draw.vp[2] = 320.f;
        draw.vp[3] = 480.f;
        CVert verts[3] = {0};
        for (size_t j = 0; j < 3; ++j) verts[j].w = 1;
        verts[0].x = -0.5f; verts[1].x = 0.5f; verts[2].y = 0.5f;
        verts[0].diffuse = 0x80FF0000u;
        verts[1].diffuse = 0xFF00FF00u;
        verts[2].diffuse = 0xFF0000FFu;
        memcpy(stream + offset, &draw, sizeof draw);
        memcpy(stream + offset + sizeof draw, verts, sizeof verts);
        offset += stride;
    }
    CmdPresent present = {0};
    present.h.type = CMD_PRESENT; present.h.size = sizeof(present);
    present.frame = 17;
    memcpy(stream + offset, &present, sizeof present); offset += sizeof present;

    float color[4] = {0}, xyzw[24] = {0};
    size_t nverts = 0, ncalls = 0;
    unsigned int frame = 0;
    Cricket07DrawCall calls[2] = {0};
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(cricket07_take_viewport_triangle_frame(color, 4, xyzw, 24,
                                                  &nverts, &frame, calls, 2,
                                                  &ncalls, 1280, 960));
    assert(nverts == 6 && ncalls == 2 && frame == 17);
    assert(calls[0].first_vertex == 0 && calls[0].vertex_count == 3);
    assert(calls[1].first_vertex == 3 && calls[1].vertex_count == 3);
    assert(fabs(calls[0].viewport.x) < 0.001);
    assert(fabs(calls[1].viewport.x - 640.) < 0.001);
    assert(fabs(calls[0].viewport.width - 640.) < 0.001);
    assert(fabs(calls[1].viewport.width - 640.) < 0.001);
    assert(fabs(calls[0].viewport.height - 960.) < 0.001);
    assert(calls[0].state.depth_compare == 8 && calls[0].state.depth_write == 0);
    assert(calls[0].state.cull_mode == 1);
    assert(calls[0].state.color_write_mask == 15 && calls[0].state.blend == 0);
    assert(calls[0].state.src_blend == 2 && calls[0].state.dst_blend == 1);
    assert(calls[1].state.depth_compare == 5 && calls[1].state.depth_write == 1);
    assert(calls[1].state.cull_mode == 2);
    assert(calls[1].state.color_write_mask == 10 && calls[1].state.blend == 1);
    assert(calls[1].state.src_blend == 5 && calls[1].state.dst_blend == 6);
    assert(calls[1].state.blend_op == 1);
    assert(fabs(xyzw[0] - (-0.5f + 1.f / 640.f)) < 0.001);

    /* Opt-in diffuse path must preserve 0xAARRGGBB per vertex for two
     * independent draws, without interpreting them as texture/shader output. */
    float diffuse[24] = {0};
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(cricket07_take_colored_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 24, &nverts, &frame,
        calls, 2, &ncalls, 1280, 960));
    assert(nverts == 6 && ncalls == 2);
    assert(fabsf(diffuse[0] - 1.0f) < 0.0001f);
    assert(diffuse[1] == 0.0f && diffuse[2] == 0.0f);
    assert(fabsf(diffuse[3] - 128.0f / 255.0f) < 0.0001f);
    assert(diffuse[4] == 0.0f && diffuse[5] == 1.0f);
    assert(diffuse[10] == 1.0f && diffuse[11] == 1.0f);
    assert(fabsf(diffuse[12] - 1.0f) < 0.0001f);

    /* UV0 staging must preserve recorded texture coordinates, not generate
     * them from transformed screen positions. Textured draws remain rejected. */
    const size_t first_vertex_offset = sizeof(CmdTarget) + sizeof(CmdClear) + sizeof(CmdDraw);
    CVert uv_vertex;
    memcpy(&uv_vertex, stream + first_vertex_offset, sizeof uv_vertex);
    uv_vertex.tc[0][0] = 0.25f;
    uv_vertex.tc[0][1] = 0.75f;
    memcpy(stream + first_vertex_offset, &uv_vertex, sizeof uv_vertex);
    float uv[12] = {0};
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(cricket07_take_colored_uv_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 24, uv, 12,
        &nverts, &frame, calls, 2, &ncalls, 1280, 960));
    assert(nverts == 6 && ncalls == 2);
    assert(fabsf(uv[0] - 0.25f) < 0.0001f);
    assert(fabsf(uv[1] - 0.75f) < 0.0001f);
    assert(uv[2] == 0.0f && uv[11] == 0.0f);
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_colored_uv_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 24, uv, 11,
        &nverts, &frame, calls, 2, &ncalls, 1280, 960));
    uv_vertex.tc[0][0] = NAN;
    memcpy(stream + first_vertex_offset, &uv_vertex, sizeof uv_vertex);
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_colored_uv_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 24, uv, 12,
        &nverts, &frame, calls, 2, &ncalls, 1280, 960));
    uv_vertex.tc[0][0] = 0.25f;
    memcpy(stream + first_vertex_offset, &uv_vertex, sizeof uv_vertex);

    /* Color buffer under-capacity must fail without writing past it. */
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_colored_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 23, &nverts, &frame,
        calls, 2, &ncalls, 1280, 960));

    /* Do not silently render a texture-bound draw as diffuse-only. */
    const size_t second_offset = sizeof(CmdTarget) + sizeof(CmdClear) + stride;
    CmdDraw texture_draw = {0};
    memcpy(&texture_draw, stream + second_offset, sizeof texture_draw);
    texture_draw.slot_kind[0] = TS_TEX;
    memcpy(stream + second_offset, &texture_draw, sizeof texture_draw);
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_colored_viewport_triangle_frame(
        color, 4, xyzw, 24, diffuse, 24, &nverts, &frame,
        calls, 2, &ncalls, 1280, 960));
    texture_draw.slot_kind[0] = TS_NONE;
    memcpy(stream + second_offset, &texture_draw, sizeof texture_draw);

    /* Reject a second draw when the metadata array can hold only one. */
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_viewport_triangle_frame(color, 4, xyzw, 24,
                                                   &nverts, &frame, calls, 1,
                                                   &ncalls, 1280, 960));
    /* Reject an invalid depth comparison without accepting a partial frame. */
    const size_t second_draw_offset = sizeof(CmdTarget) + sizeof(CmdClear) + stride;
    CmdDraw invalid = {0};
    memcpy(&invalid, stream + second_draw_offset, sizeof invalid);
    invalid.key.zfunc = 9u;
    memcpy(stream + second_draw_offset, &invalid, sizeof invalid);
    assert(cricket07_frame_queue_publish(stream, offset));
    assert(!cricket07_take_viewport_triangle_frame(color, 4, xyzw, 24,
                                                   &nverts, &frame, calls, 2,
                                                   &ncalls, 1280, 960));

    Cricket07DrawState state = {0};
    assert(!cricket07_resolve_draw_state(NULL, &state));
    invalid.key.zfunc = 4u;
    invalid.key.cull = 4u;
    assert(!cricket07_resolve_draw_state(&invalid, &state));
    invalid.key.cull = 1u;
    assert(cricket07_resolve_draw_state(&invalid, &state));
    assert(state.cull_mode == 0 && state.depth_compare == 4);
    invalid.key.cmask = 16u;
    assert(!cricket07_resolve_draw_state(&invalid, &state));
    invalid.key.cmask = 15u;
    invalid.key.blend = 1u;
    invalid.key.src = 14u;
    assert(!cricket07_resolve_draw_state(&invalid, &state));
    invalid.key.src = 5u;
    invalid.key.dst = 11u; /* source-alpha-saturate is not a destination factor */
    assert(!cricket07_resolve_draw_state(&invalid, &state));
    invalid.key.dst = 6u;
    invalid.key.op = 6u;
    assert(!cricket07_resolve_draw_state(&invalid, &state));
    invalid.key.op = 5u;
    assert(cricket07_resolve_draw_state(&invalid, &state));
    assert(state.blend == 1 && state.blend_op == 5);

    cricket07_frame_queue_reset();
    puts("iOS multi-viewport draw regression passed");
    return 0;
}
