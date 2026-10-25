#include "VertexTransform.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int near(float a, float b) { return fabsf(a - b) < 0.0001f; }

int main(void) {
    CmdDraw draw = {0};
    draw.key.topo = 4;
    draw.nverts = 3;
    draw.u.vp[2] = 640.f;
    draw.u.vp[3] = 480.f;
    float in[12] = {0.f, 0.f, 0.25f, 1.f,
                    0.5f, 0.f, 0.25f, 1.f,
                    0.f, 0.5f, 0.25f, 1.f};
    float out[12] = {0};

    /* Already-clip-space vertex: half-pixel correction from WGSL. */
    draw.u.flags[0] = 2;
    assert(cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    assert(near(out[0], 1.f / 640.f));
    assert(near(out[1], -1.f / 480.f));
    assert(near(out[2], 0.25f) && near(out[3], 1.f));

    /* Pre-transformed D3D screen coordinates and reciprocal W. */
    draw.u.flags[0] = 1;
    draw.u.vp[0] = 10.f; draw.u.vp[1] = 20.f;
    in[0] = 329.5f; in[1] = 259.5f; in[3] = 2.f;
    assert(cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    assert(near(out[0], 0.f) && near(out[1], 0.f));
    assert(near(out[2], 0.125f) && near(out[3], 0.5f));

    /* Object-space vertex: column-major MVP and half-pixel correction. */
    draw.u.flags[0] = 0;
    draw.u.vp[0] = draw.u.vp[1] = 0.f;
    in[0] = 0.f; in[1] = 0.f; in[3] = 1.f;
    for (size_t i = 0; i < 16; ++i) draw.u.mvp[i] = 0.f;
    for (size_t i = 0; i < 4; ++i) draw.u.mvp[i * 5] = 1.f;
    draw.u.mvp[12] = 2.f; draw.u.mvp[13] = -1.f;
    assert(cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    assert(near(out[0], 2.f + 1.f / 640.f));
    assert(near(out[1], -1.f - 1.f / 480.f));
    assert(near(out[3], 1.f));

    /* Invalid draws never overwrite the caller's destination. */
    for (size_t i = 0; i < 12; ++i) out[i] = -123.f;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 11));
    assert(out[0] == -123.f);
    draw.u.vp[2] = 0.f;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    draw.u.vp[2] = 640.f;
    draw.u.mvp[0] = NAN;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    draw.u.mvp[0] = 1.f;
    in[5] = INFINITY;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    assert(out[0] == -123.f);
    in[5] = 0.f;
    draw.u.flags[0] = 3;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    draw.u.flags[0] = 2;
    draw.nverts = 6;
    assert(!cricket07_transform_draw_positions(&draw, in, 3, out, 12));
    assert(out[0] == -123.f);
    puts("Vertex transform reference tests passed");
    return 0;
}
