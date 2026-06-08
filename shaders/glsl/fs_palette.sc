// Caesura (AmeKAG) - 3D LUT Palette Fragment Shader (GLSL)
// bgfx shaderc variant
// Applies a 256x16 (16x16x16) 3D LUT unfolded into a 2D texture.
// Standard industry format: 4096x64 or 256x16 texture as lookup.

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_inputTex, 0);   // Source image
SAMPLER2D(s_lutTex,   1);   // 3D LUT unfolded to 2D (256x16 pixels for 16^3)

uniform vec4 u_paletteParams;  // x=intensity (0-1), y=LUT size (16), z/w=unused
#define u_intensity u_paletteParams.x
#define u_lutSize   u_paletteParams.y  // typically 16.0

// -- Tetrahedral interpolation helpers -------------------------------------
// Converts a 2D LUT coordinate into a 3D index for tetrahedral lookup

vec4 lutLookup(vec3 texColor, sampler2D lut, float size) {
    // Scale from [0,1] to LUT index space
    float maxCoord = size - 1.0;
    vec3 scaled = texColor * maxCoord;

    // Base cell index
    vec3 idx0 = floor(scaled);
    vec3 idx1 = min(idx0 + vec3(1.0), vec3(maxCoord));
    vec3 frac  = scaled - idx0;

    // Convert 3D LUT index → 2D texture coordinate
    // Layout: blue varies fastest, then green, then red.
    // Each "row" is size cells wide; each "tile" is size rows tall.
    // 2D texture is (size*size) wide × size tall.
    float tilesX = size;       // blue tiles per row
    float tilesY = size;       // green tiles per column

    auto lutCoord = [&](vec3 idx) -> vec2 {
        // x = blue_index + red_index * size
        // y = green_index
        float u = (idx.b + idx.r * tilesX + 0.5) / (tilesX * size);
        float v = (idx.g + 0.5) / tilesY;
        return vec2(u, v);
    };

    // 8 corners of the cube
    vec4 c000 = texture2D(lut, lutCoord(vec3(idx0.r, idx0.g, idx0.b)));
    vec4 c100 = texture2D(lut, lutCoord(vec3(idx1.r, idx0.g, idx0.b)));
    vec4 c010 = texture2D(lut, lutCoord(vec3(idx0.r, idx1.g, idx0.b)));
    vec4 c110 = texture2D(lut, lutCoord(vec3(idx1.r, idx1.g, idx0.b)));
    vec4 c001 = texture2D(lut, lutCoord(vec3(idx0.r, idx0.g, idx1.b)));
    vec4 c101 = texture2D(lut, lutCoord(vec3(idx1.r, idx0.g, idx1.b)));
    vec4 c011 = texture2D(lut, lutCoord(vec3(idx0.r, idx1.g, idx1.b)));
    vec4 c111 = texture2D(lut, lutCoord(vec3(idx1.r, idx1.g, idx1.b)));

    // Trilinear interpolation
    vec4 c00 = mix(c000, c100, frac.r);
    vec4 c01 = mix(c001, c101, frac.r);
    vec4 c10 = mix(c010, c110, frac.r);
    vec4 c11 = mix(c011, c111, frac.r);

    vec4 c0 = mix(c00, c10, frac.g);
    vec4 c1 = mix(c01, c11, frac.g);

    return mix(c0, c1, frac.b);
}

void main()
{
    vec4 inputColor = texture2D(s_inputTex, v_texcoord0);
    vec4 lutResult  = lutLookup(inputColor.rgb, s_lutTex, u_lutSize);
    // Bugfix #1: LUT only modifies RGB; preserve the original alpha channel.
    // The LUT texture's alpha channel is arbitrary — mixing it would corrupt
    // the source image's transparency.
    vec3 finalRGB = mix(inputColor.rgb, lutResult.rgb, u_intensity);
    gl_FragColor = vec4(finalRGB, inputColor.a);
}