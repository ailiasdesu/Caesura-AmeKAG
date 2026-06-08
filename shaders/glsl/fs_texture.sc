// Caesura (AmeKAG) - 2D Texture Fragment Shader (GLSL)
// bgfx shaderc variant
// Corresponds to shaders/dx11/fs_texture.hlsl

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texture, 0);

// Bugfix #3+#4: uniform for per-draw color tint (rgb) and opacity (a).
// Set to (1,1,1,1) for default passthrough; modulated for text color
// or per-quad batch opacity.  Default in bgfx without setUniform is zero,
// so every call site that uses this shader MUST set u_texColor.
uniform vec4 u_texColor;

void main()
{
    gl_FragColor = texture2D(s_texture, v_texcoord0) * u_texColor;
}
