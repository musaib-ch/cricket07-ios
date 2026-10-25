#include <metal_stdlib>
using namespace metal;

// Diagnostic triangle only. Not Cricket 07's actual shaders.
struct TriangleVertex {
    float4 position [[position]];
    float4 color;
    float2 uv;
};
vertex TriangleVertex cricket07_test_vertex(uint id [[vertex_id]]) {
    constexpr float2 coords[3] = {float2(-0.7, -0.55), float2(0.7, -0.55), float2(0.0, 0.7)};
    constexpr float4 colors[3] = {float4(1,0.3,0.2,1), float4(0.2,0.8,0.4,1), float4(0.3,0.5,1,1)};
    TriangleVertex out;
    out.position = float4(coords[id], 0, 1);
    out.color = colors[id];
    out.uv = coords[id] * 0.5 + 0.5;
    return out;
}
fragment float4 cricket07_test_fragment(TriangleVertex in [[stage_in]]) {
    return in.color;
}

// Diagnostic unlit diffuse colors, not fixed-function stage emulation.
struct StagedVertex { float4 position; float4 color; float2 uv; };
vertex TriangleVertex cricket07_staged_vertex(const device StagedVertex *vertices [[buffer(0)]],
                                             uint id [[vertex_id]]) {
    TriangleVertex out;
    out.position = vertices[id].position;
    out.color = vertices[id].color;
    // C validates and stages the recorded CVert.tc[0].xy values.
    // Texture stage semantics and game texture binding remain unsupported.
    out.uv = vertices[id].uv;
    return out;
}

// Samples a generated test texture on the synthetic triangle only.
// Real D3D8 texture coordinates, stage combiners and bindings are not yet ported.
fragment float4 cricket07_test_textured_fragment(
    TriangleVertex in [[stage_in]],
    texture2d<float> diagnosticTexture [[texture(0)]]) {
    constexpr sampler nearestSampler(coord::normalized,
                                     address::clamp_to_edge,
                                     filter::nearest,
                                     mip_filter::nearest);
    return in.color * diagnosticTexture.sample(nearestSampler, in.uv);
}
