// Caesura (AmeKAG) - Affine Blit Vertex Shader (Metal)
// 2D affine matrix transform
// Corresponds to spec [2.4] affine_blt.vs
//
// Matrix layout (row-major 2x3, matching spec):
//   | a  c  tx |    u_affineParams[0] = { a, c, tx, 0 }
//   | b  d  ty |    u_affineParams[1] = { b, d, ty, 0 }
//   u_affineParams[2] = { sx, sy, sw, sh }
//   u_affineParams[3] = padding

#include <metal_stdlib>
using namespace metal;

struct VSInput {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VSOutput {
    float4 position [[position]];
    float2 texcoord;
};

struct AffineParams {
    float4x4 u_affineParams;
};

vertex VSOutput xlatMtlMain(VSInput in [[stage_in]],
                              constant AffineParams& params [[buffer(0)]]) {
    VSOutput out;

    float2 pos = in.position;

    // Row 0: { a, c, tx, _ }
    float a  = params.u_affineParams[0].x;
    float c  = params.u_affineParams[0].y;
    float tx = params.u_affineParams[0].z;

    // Row 1: { b, d, ty, _ }
    float b  = params.u_affineParams[1].x;
    float d  = params.u_affineParams[1].y;
    float ty = params.u_affineParams[1].z;

    // x`` = a*x + c*y + tx,  y`` = b*x + d*y + ty
    float2 tp;
    tp.x = a * pos.x + c * pos.y + tx;
    tp.y = b * pos.x + d * pos.y + ty;

    float4 srcR = params.u_affineParams[2];
    out.texcoord = srcR.xy + tp * srcR.zw;
    out.position = float4(in.position * 2.0 - 1.0, 0.0, 1.0);

    return out;
}
