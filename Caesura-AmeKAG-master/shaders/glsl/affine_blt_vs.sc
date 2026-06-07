// Caesura (AmeKAG) - Affine Blit Vertex Shader (GLSL)
// bgfx shaderc variant: 2D affine matrix transform
// Corresponds to spec [2.4] affine_blt.vs
//
// Matrix layout (row-major 2x3, matching spec):
//   | a  c  tx |    u_affineParams[0] = { a, c, tx, 0 }
//   | b  d  ty |    u_affineParams[1] = { b, d, ty, 0 }
//   u_affineParams[2] = { sx, sy, sw, sh }  (src rect)
//   u_affineParams[3] = padding

$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_affineParams[4];

void main()
{
    vec2 pos = a_position;

    // Row 0: { a, c, tx, _ }
    float a  = u_affineParams[0].x;
    float c  = u_affineParams[0].y;
    float tx = u_affineParams[0].z;

    // Row 1: { b, d, ty, _ }
    float b  = u_affineParams[1].x;
    float d  = u_affineParams[1].y;
    float ty = u_affineParams[1].z;

    // x`` = a*x + c*y + tx,  y`` = b*x + d*y + ty
    vec2 tp;
    tp.x = a * pos.x + c * pos.y + tx;
    tp.y = b * pos.x + d * pos.y + ty;

    // Apply src_rect offset/scale
    vec4 srcR = u_affineParams[2];
    v_texcoord0 = srcR.xy + tp * srcR.zw;

    gl_Position = vec4(a_position * 2.0 - 1.0, 0.0, 1.0);
}
