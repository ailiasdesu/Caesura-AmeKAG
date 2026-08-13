// Caesura (AmeKAG) - SMA S5 skin draw vertex shader (vs_4_0)
// Applies the draw transform (x, y, scale) + pixel->NDC conversion on the
// GPU (CPU path does the same math per vertex). u_skinDraw/View live in
// the per-shader constant buffer bound at b0 (regIndex*16 layout).
cbuffer SkinParams : register(b0) {
    float4 u_skinDraw;  // x, y, scale, opacity
    float4 u_skinView;  // width, height
};

struct VSInput {
    float2 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    float2 p = float2(u_skinDraw.x + input.position.x * u_skinDraw.z,
                      u_skinDraw.y + input.position.y * u_skinDraw.z);
    output.position = float4(p.x / u_skinView.x * 2.0 - 1.0,
                             1.0 - p.y / u_skinView.y * 2.0, 0.0, 1.0);
    output.texcoord = input.texcoord;
    return output;
}
