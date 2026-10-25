#include <metal_stdlib>
using namespace metal;

// Diagnostic untextured triangle only. Not Cricket 07's actual shaders.
struct TriangleVertex {
    float4 position [[position]];
    float4 color;
};
vertex TriangleVertex cricket07_test_vertex(uint id [[vertex_id]]) {
    constexpr float2 coords[3] = {float2(-0.7, -0.55), float2(0.7, -0.55), float2(0.0, 0.7)};
    constexpr float4 colors[3] = {float4(1,0.3,0.2,1), float4(0.2,0.8,0.4,1), float4(0.3,0.5,1,1)};
    TriangleVertex out;
    out.position = float4(coords[id], 0, 1);
    out.color = colors[id];
    return out;
}
fragment float4 cricket07_test_fragment(TriangleVertex in [[stage_in]]) {
    return in.color;
}

struct StagedVertex { float4 position; };
vertex TriangleVertex cricket07_staged_vertex(const device StagedVertex *vertices [[buffer(0)]],
                                             uint id [[vertex_id]]) {
    TriangleVertex out;
    out.position = vertices[id].position;
    uint corner = id % 3;
    out.color = float4(0.2 + float(corner == 0) * 0.8,
                       0.2 + float(corner == 1) * 0.8,
                       0.2 + float(corner == 2) * 0.8, 1.0);
    return out;
}
