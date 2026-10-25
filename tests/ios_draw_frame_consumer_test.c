#include "RuntimeBridge.h"
#include "FrameQueue.h"
#include "render_cmd.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    cricket07_frame_queue_reset();
    float rgba[4] = {0};
    float xyzw[12] = {0};
    size_t count = 0;
    unsigned int frame = 0;

    /* Exercise a complete producer -> mailbox -> parser frame. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 1);
    assert(count == 3 && frame == 7);
    assert(fabsf(xyzw[0] + 0.7f) < 0.0001f);
    assert(fabsf(xyzw[4] - 0.7f) < 0.0001f);
    assert(fabsf(xyzw[9] - 0.7f) < 0.0001f);
    assert(xyzw[3] == 1.0f && xyzw[7] == 1.0f && xyzw[11] == 1.0f);
    assert(fabsf(rgba[0] - 22.0f / 255.0f) < 0.0001f);
    assert(fabsf(rgba[1] - 96.0f / 255.0f) < 0.0001f);
    assert(fabsf(rgba[2] - 120.0f / 255.0f) < 0.0001f);
    assert(rgba[3] == 1.0f);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);

    /* The Metal consumer must use transformed clip-space positions, not raw D3D
     * coordinates. Mode 2 applies the half-pixel correction from the WGSL VS. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_transformed_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 1);
    assert(count == 3 && frame == 7);
    assert(fabsf(xyzw[0] - (-0.7f + 1.f / 640.f)) < 0.0001f);
    assert(fabsf(xyzw[1] - (-0.55f - 1.f / 480.f)) < 0.0001f);
    assert(fabsf(xyzw[4] - (0.7f + 1.f / 640.f)) < 0.0001f);
    assert(xyzw[3] == 1.f && xyzw[7] == 1.f && xyzw[11] == 1.f);

    /* The Metal path retains each draw's own viewport and vertex range. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    Cricket07DrawCall calls[2] = {0};
    size_t call_count = 0;
    assert(cricket07_take_viewport_triangle_frame(rgba, 4, xyzw, 12, &count,
                                                  &frame, calls, 2, &call_count,
                                                  1280, 960) == 1);
    assert(count == 3 && call_count == 1 && frame == 7);
    assert(calls[0].first_vertex == 0 && calls[0].vertex_count == 3);
    assert(fabs(calls[0].viewport.width - 1280.) < 0.001);
    assert(fabs(calls[0].viewport.height - 960.) < 0.001);
    assert(calls[0].viewport.min_depth == 0 && calls[0].viewport.max_depth == 1);
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_viewport_triangle_frame(rgba, 4, xyzw, 12, &count,
                                                  &frame, calls, 0, &call_count,
                                                  1280, 960) == 0);
    cricket07_frame_queue_reset();

    /* Two sequential CMD_DRAW commands must combine into one Metal buffer. */
    {
        const size_t draw_size = (sizeof(CmdDraw) + 3u * sizeof(CVert) + 7u) & ~(size_t)7u;
        unsigned char stream[sizeof(CmdClear) + 2u * (sizeof(CmdDraw) + 3u * sizeof(CVert) + 8u)
                             + sizeof(CmdPresent)] = {0};
        CmdClear clear = {0};
        clear.h.type = CMD_CLEAR;
        clear.h.size = (uint32_t)sizeof(clear);
        clear.flags = 1u;
        clear.color = 0xFF166078u;
        size_t offset = 0;
        memcpy(stream + offset, &clear, sizeof clear);
        offset += sizeof clear;
        for (unsigned int d = 0; d < 2; ++d) {
            CmdDraw draw = {0};
            draw.h.type = CMD_DRAW;
            draw.h.size = (uint32_t)draw_size;
            draw.key.topo = 4u;
            draw.nverts = 3u;
            CVert verts[3] = {0};
            verts[0].x = d == 0 ? -0.8f : 0.1f;
            verts[1].x = d == 0 ? -0.1f : 0.8f;
            verts[2].y = 0.8f;
            for (size_t v = 0; v < 3; ++v) verts[v].w = 1.0f;
            memcpy(stream + offset, &draw, sizeof draw);
            memcpy(stream + offset + sizeof draw, verts, sizeof verts);
            offset += draw_size;
        }
        CmdPresent present = {0};
        present.h.type = CMD_PRESENT;
        present.h.size = (uint32_t)sizeof(present);
        present.frame = 8u;
        memcpy(stream + offset, &present, sizeof present);
        offset += sizeof present;
        float multi[24] = {0};
        assert(cricket07_frame_queue_publish(stream, offset) == 1);
        assert(cricket07_take_triangle_frame(rgba, 4, multi, 24, &count, &frame) == 1);
        assert(count == 6 && frame == 8u);
        assert(fabsf(multi[0] + 0.8f) < 0.0001f);
        assert(fabsf(multi[12] - 0.1f) < 0.0001f);
        assert(multi[23] == 1.0f);
        /* Zero viewport dimensions are invalid for the transformed path.
         * The legacy raw staging API above intentionally still accepts them. */
        assert(cricket07_frame_queue_publish(stream, offset) == 1);
        assert(cricket07_take_transformed_triangle_frame(rgba, 4, multi, 24, &count, &frame) == 0);
        /* A capacity of only five vertices must reject the second draw. */
        assert(cricket07_frame_queue_publish(stream, offset) == 1);
        assert(cricket07_take_triangle_frame(rgba, 4, multi, 20, &count, &frame) == 0);
    }

    /* A too-small destination must not overrun and must reject the draw. */
    assert(cricket07_submit_test_triangle_frame() == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 11, &count, &frame) == 0);

    /* Malformed and unknown commands must fail closed. */
    CmdHdr bad = { CMD_DRAW, UINT32_MAX };
    assert(cricket07_frame_queue_publish(&bad, sizeof bad) == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);
    assert(cricket07_frame_queue_publish("bad", 3) == 1);
    assert(cricket07_take_triangle_frame(rgba, 4, xyzw, 12, &count, &frame) == 0);

    /* Null output and undersized color destination are rejected. */
    assert(cricket07_take_triangle_frame(NULL, 4, xyzw, 12, &count, &frame) == 0);
    assert(cricket07_take_triangle_frame(rgba, 3, xyzw, 12, &count, &frame) == 0);
    cricket07_frame_queue_reset();

    /* The iOS app now exercises CMD_UPLOAD_TEX through the real mailbox,
     * instead of bypassing the frame decoder with a Swift-side upload. */
    {
        float colors[12] = {0}, uv[6] = {0};
        Cricket07DrawCall textured_calls[2] = {0};
        size_t textured_count = 0, command_bytes = 0;
        unsigned char commands[256] = {0};
        assert(cricket07_submit_test_uploaded_texture_frame() == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_count, 1280, 960,
            commands, sizeof commands, &command_bytes) == 1);
        assert(count == 3 && frame == 7 && textured_count == 1);
        assert(textured_calls[0].texture_id == 1u);
        Cricket07TextureUpload decoded = {0};
        assert(cricket07_decode_texture_upload(commands, command_bytes, &decoded) == 1);
        assert(decoded.texture_id == 1u && decoded.levels == 2u);
        assert(decoded.width[0] == 2u && decoded.height[0] == 2u);
        assert(decoded.width[1] == 1u && decoded.height[1] == 1u);
        assert(fabsf(uv[1] - 1.f) < 0.0001f);
        /* Legacy consumers must not silently discard the new upload. */
        assert(cricket07_submit_test_uploaded_texture_frame() == 1);
        assert(cricket07_take_colored_uv_viewport_triangle_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_count, 1280, 960) == 0);
    }

    /* Real recorder texture commands must be copied out of a validated
     * frame before Metal consumes its draw calls. The payload is synthetic. */
    {
        unsigned char base[4096] = {0}, stream[4096] = {0};
        unsigned char upload[256] = {0}, extracted[256] = {0};
        size_t upload_size = 0, extracted_size = 0;
        float colors[12] = {0}, uv[6] = {0};
        Cricket07DrawCall textured_calls[2] = {0};
        size_t textured_call_count = 0;
        assert(cricket07_submit_test_triangle_frame() == 1);
        size_t base_size = cricket07_frame_queue_take(base, sizeof base);
        assert(base_size > sizeof(CmdPresent));
        assert(cricket07_make_test_texture_upload(upload, sizeof upload, &upload_size) == 1);
        assert(upload_size + base_size < sizeof stream);
        memcpy(stream, upload, upload_size);
        memcpy(stream + upload_size, base, base_size);
        assert(cricket07_frame_queue_publish(stream, upload_size + base_size) == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_call_count, 1280, 960,
            extracted, sizeof extracted, &extracted_size) == 1);
        assert(count == 3 && frame == 7 && textured_call_count == 1);
        assert(textured_calls[0].texture_id == 1);
        assert(extracted_size == upload_size);
        assert(memcmp(extracted, upload, upload_size) == 0);
        Cricket07TextureUpload decoded = {0};
        assert(cricket07_decode_texture_upload(extracted, extracted_size, &decoded) == 1);
        assert(decoded.texture_id == 1 && decoded.levels == 2);
        assert(fabsf(uv[1] - 1.0f) < 0.0001f);

        /* Insufficient texture output capacity fails closed. */
        assert(cricket07_frame_queue_publish(stream, upload_size + base_size) == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_call_count, 1280, 960,
            extracted, upload_size - 1, &extracted_size) == 0);

        /* A malformed mip chain must never be handed to Metal. */
        memset(stream + 12, 0, 4); /* CMD_UPLOAD_TEX.levels */
        assert(cricket07_frame_queue_publish(stream, upload_size + base_size) == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_call_count, 1280, 960,
            extracted, sizeof extracted, &extracted_size) == 0);

        /* Destroy commands are preserved, but only before any draw. */
        memcpy(stream, upload, upload_size);
        CmdDestroy destroy = {0};
        destroy.h.type = CMD_DESTROY_TEX;
        destroy.h.size = (uint32_t)((sizeof destroy + 7u) & ~(size_t)7u);
        destroy.id = 9;
        unsigned char padded_destroy[16] = {0};
        memcpy(padded_destroy, &destroy, sizeof destroy);
        memcpy(stream + upload_size, padded_destroy, sizeof padded_destroy);
        memcpy(stream + upload_size + sizeof padded_destroy, base, base_size);
        assert(cricket07_frame_queue_publish(stream, upload_size + sizeof padded_destroy + base_size) == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_call_count, 1280, 960,
            extracted, sizeof extracted, &extracted_size) == 1);
        assert(extracted_size == upload_size + sizeof padded_destroy);
        assert(memcmp(extracted + upload_size, padded_destroy, sizeof padded_destroy) == 0);

        /* An upload after a draw would change per-draw ordering: reject it. */
        memcpy(stream, base, base_size - sizeof(CmdPresent));
        memcpy(stream + base_size - sizeof(CmdPresent), upload, upload_size);
        memcpy(stream + base_size - sizeof(CmdPresent) + upload_size,
               base + base_size - sizeof(CmdPresent), sizeof(CmdPresent));
        assert(cricket07_frame_queue_publish(stream, base_size + upload_size) == 1);
        assert(cricket07_take_colored_uv_texture_frame(
            rgba, 4, xyzw, 12, colors, 12, uv, 6, &count, &frame,
            textured_calls, 2, &textured_call_count, 1280, 960,
            extracted, sizeof extracted, &extracted_size) == 0);
    }
    puts("iOS draw frame consumer regression passed");
    return 0;
}
