#include "ViewportBridge.h"
#include <math.h>
#include <stddef.h>

/* A draw's vp is in logical target pixels; the GPU viewport is in physical
 * target pixels. Clamp to the backbuffer, including negative origins.
 * Reject invalid depth intervals rather than submitting a Metal command.
 */
int cricket07_resolve_viewport(const CmdDraw *draw,
                              uint32_t target_width, uint32_t target_height,
                              uint32_t logical_width, uint32_t logical_height,
                              Cricket07Viewport *out) {
    if (!draw || !out || !target_width || !target_height ||
        !logical_width || !logical_height ||
        target_width > 16384u || target_height > 16384u ||
        logical_width > 16384u || logical_height > 16384u ||
        !isfinite(draw->minz) || !isfinite(draw->maxz) ||
        draw->minz < 0.f || draw->maxz > 1.f ||
        draw->minz > draw->maxz) return 0;

    double x = 0, y = 0;
    double right = (double)target_width;
    double bottom = (double)target_height;
    if (!draw->full_viewport) {
        for (size_t i = 0; i < 4; ++i)
            if (!isfinite(draw->vp[i])) return 0;
        if (draw->vp[2] <= 0.f || draw->vp[3] <= 0.f) return 0;
        const double sx = (double)target_width / logical_width;
        const double sy = (double)target_height / logical_height;
        x = (double)draw->vp[0] * sx;
        y = (double)draw->vp[1] * sy;
        right = ((double)draw->vp[0] + draw->vp[2]) * sx;
        bottom = ((double)draw->vp[1] + draw->vp[3]) * sy;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (right > target_width) right = target_width;
        if (bottom > target_height) bottom = target_height;
    }
    if (x >= right || y >= bottom) return 0;
    Cricket07Viewport staged = {x, y, right - x, bottom - y,
                                draw->minz, draw->maxz};
    *out = staged;
    return 1;
}

/* Direct3D 8 depth comparisons are 1..8. The existing WebGPU backend
 * disables depth writes when depth testing is disabled, regardless of zwrite.
 * Culling matches its clockwise-front convention.
 */
int cricket07_resolve_draw_state(const CmdDraw *draw, Cricket07DrawState *out) {
    if (!draw || !out || draw->key.zen > 1u || draw->key.zwrite > 1u ||
        draw->key.cull > 3u || draw->key.cmask > 15u ||
        draw->key.blend > 1u ||
        (draw->key.blend &&
         (draw->key.src < 1u || draw->key.src > 11u ||
          draw->key.dst < 1u || draw->key.dst > 10u ||
          draw->key.op < 1u || draw->key.op > 5u)) ||
        (draw->key.zen && (draw->key.zfunc < 1u || draw->key.zfunc > 8u)))
        return 0;
    Cricket07DrawState state = {0};
    state.depth_compare = draw->key.zen ? draw->key.zfunc : 8u;
    state.depth_write = draw->key.zen && draw->key.zwrite ? 1u : 0u;
    state.cull_mode = draw->key.cull == 2u ? 1u : draw->key.cull == 3u ? 2u : 0u;
    state.blend = draw->key.blend;
    state.src_blend = draw->key.blend ? draw->key.src : 2u;
    state.dst_blend = draw->key.blend ? draw->key.dst : 1u;
    state.blend_op = draw->key.blend ? draw->key.op : 1u;
    state.color_write_mask = draw->key.cmask;
    *out = state;
    return 1;
}

/* Stage-0 D3DTOP_MODULATE only: texture * diffuse/current.
 * Direct3D argument 2 is TEXTURE; 0 and 1 are DIFFUSE/CURRENT.
 * This deliberately does not attempt general fixed-function emulation.
 * Sampler key 432 (D3DTADDRESS_CLAMP for U/V, nearest filtering,
 * mip disabled) matches the Metal diagnostic sampler. Missing/unsupported stages must not be misrendered. */
int cricket07_resolve_texture_binding(const CmdDraw *draw, uint32_t *texture_id) {
    if (!draw || !texture_id) return 0;
    *texture_id = 0;
    for (unsigned i = 1; i < 4; ++i)
        if (draw->slot_kind[i] != TS_NONE) return 0;
    if (draw->slot_kind[0] == TS_NONE) return 1;
    if (draw->slot_kind[0] != TS_TEX || !draw->slot_id[0] ||
        draw->pixel_shader || draw->u.flags[1] ||
        draw->u.flags[2] || draw->u.flags[3] ||
        draw->samp_key[0] != 432u || draw->u.st1[0] != 1 ||
        draw->u.st0[0] != 4 || draw->u.st0[1] != 2 ||
        (draw->u.st0[2] != 0 && draw->u.st0[2] != 1) ||
        draw->u.st0[3] != 4 || draw->u.st0b[0] != 2 ||
        (draw->u.st0b[1] != 0 && draw->u.st0b[1] != 1) ||
        draw->u.st0b[2] != 0) return 0;
    *texture_id = draw->slot_id[0];
    return 1;
}
