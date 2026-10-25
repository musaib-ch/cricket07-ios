#include "VertexTransform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Mirrors the POSITION portion of the existing WGSL vertex shader.
 * WGSL mat4x4 is column-major; D3D pre-transformed RHW is reciprocal W.
 * The draw's GPU viewport is a separate operation, not implemented here.
 */
int cricket07_transform_draw_positions(const CmdDraw *draw,
                                       const float *input_xyzw,
                                       size_t vertex_count,
                                       float *output_xyzw,
                                       size_t output_float_capacity) {
    if (!draw || !input_xyzw || !output_xyzw || !vertex_count ||
        vertex_count > 65535u || vertex_count % 3u != 0u ||
        vertex_count != draw->nverts || draw->key.topo != 4u ||
        vertex_count > output_float_capacity / 4u) return 0;

    const unsigned mode = draw->u.flags[0];
    const float *vp = draw->u.vp;
    if (mode > 2u || !isfinite(vp[2]) || !isfinite(vp[3]) ||
        vp[2] <= 0.f || vp[3] <= 0.f) return 0;
    if (mode == 1u && (!isfinite(vp[0]) || !isfinite(vp[1]))) return 0;
    if (mode == 0u) {
        for (size_t i = 0; i < 16; ++i)
            if (!isfinite(draw->u.mvp[i])) return 0;
    }

    float *staged = malloc(vertex_count * 4u * sizeof(float));
    if (!staged) return 0;
    int valid = 1;
    for (size_t i = 0; i < vertex_count && valid; ++i) {
        const float *v = input_xyzw + i * 4u;
        for (size_t k = 0; k < 4u; ++k) {
            if (!isfinite(v[k])) { valid = 0; break; }
        }
        if (!valid) break;
        float p[4];
        if (mode == 1u) {
            const float w = v[3] == 0.f ? 1.f : 1.f / v[3];
            const float nx = (v[0] + 0.5f - vp[0]) / vp[2] * 2.f - 1.f;
            const float ny = 1.f - (v[1] + 0.5f - vp[1]) / vp[3] * 2.f;
            p[0] = nx * w; p[1] = ny * w; p[2] = v[2] * w; p[3] = w;
        } else {
            if (mode == 2u) {
                memcpy(p, v, sizeof p);
            } else {
                const float *m = draw->u.mvp;
                for (size_t row = 0; row < 4; ++row) {
                    p[row] = m[row] * v[0] + m[4 + row] * v[1] +
                             m[8 + row] * v[2] + m[12 + row];
                }
            }
            p[0] += p[3] / vp[2];
            p[1] -= p[3] / vp[3];
        }
        for (size_t k = 0; k < 4u; ++k) {
            if (!isfinite(p[k])) { valid = 0; break; }
            staged[i * 4u + k] = p[k];
        }
    }
    if (valid) memcpy(output_xyzw, staged, vertex_count * 4u * sizeof(float));
    free(staged);
    return valid;
}
