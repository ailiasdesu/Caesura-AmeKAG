// Caesura (AmeKAG) - SMA S5 GPU skinning compute shader (cs_5_0)
// Reads per-vertex {pos(2), uv(2), bone0/bone1(2), w0/w1(2)} (input VB
// layout stride 32B) and the bone transform buffer (vec4 per bone:
// (cos*scale, sin*scale, ox, oy); slots 64/65 carry the DRAW transform
// (x, y, scale) and the view size (sw, sh)), writes the FINAL NDC
// positions + UV (output VB stride 16B). Registers match the binding
// stages: input t0 (stage 0), bones t1 (stage 1), output u2 (stage 2).
Buffer<float4> skinInput : register(t0);
Buffer<float4> skinBones : register(t1);
RWBuffer<float4> skinOutput : register(u2);

[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    uint vid = dtid.x;
    float4 a = skinInput[vid * 2u];       // pos.xy, uv.xy
    float4 b = skinInput[vid * 2u + 1u];  // bone0, bone1, w0, w1
    float2 pos = a.xy;
    float2 uv = a.zw;
    float2 bones = b.xy;
    float2 weights = b.zw;

    float wsum = weights.x + weights.y;
    float2 skinned = pos;  // in place (CPU: no valid bone / zero weight sum)
    if (wsum > 0.0) {
        float4 m0 = skinBones[(uint)bones.x];
        float2 p0 = float2(m0.x * pos.x - m0.y * pos.y + m0.z,
                           m0.y * pos.x + m0.x * pos.y + m0.w);
        if (weights.y > 0.0 && bones.y >= 0.0 && bones.y < 64.0) {
            float4 m1 = skinBones[(uint)bones.y];
            float2 p1 = float2(m1.x * pos.x - m1.y * pos.y + m1.z,
                               m1.y * pos.x + m1.x * pos.y + m1.w);
            skinned = (p0 * weights.x + p1 * weights.y) / wsum;
        } else {
            skinned = p0;
        }
    }
    // Draw transform slots (64 = x,y,scale; 65 = view width,height):
    // pixel -> NDC exactly like the CPU path does per vertex.
    float4 drawP = skinBones[64];
    float4 viewP = skinBones[65];
    float2 px = float2(drawP.x + skinned.x * drawP.z,
                       drawP.y + skinned.y * drawP.z);
    float2 ndc = float2(px.x / viewP.x * 2.0 - 1.0,
                        1.0 - px.y / viewP.y * 2.0);
    skinOutput[vid] = float4(ndc, uv);
}
