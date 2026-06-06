// Caesura (AmeKAG) - Full 28-mode Blend Fragment Shader (GLSL)
// bgfx shaderc variant — all Photoshop-compatible blend modes
// Mode index constants: see BlendMode enum in ShaderCache.h

$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_baseTex,  0);
SAMPLER2D(s_blendTex, 1);

uniform vec4 u_blendParams;       // x=baseAlpha, y=blendAlpha, z=globalAlpha, w=mode
#define u_opacity     u_blendParams.x
#define u_blendAlpha  u_blendParams.y
#define u_globalAlpha u_blendParams.z
#define u_blendMode   int(u_blendParams.w)

// -- Helper: luminance (Rec.709) --------------------------------------------
float lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// -- Helper: set luminance while preserving hue/sat -------------------------
vec3 setLum(vec3 c, float l) {
    float d = l - lum(c);
    return clamp(vec3(c.r + d, c.g + d, c.b + d), 0.0, 1.0);
}

// -- 28 blend mode functions (Photoshop-compatible) -------------------------
vec3 bNormal(    vec3 a, vec3 b) { return b; }
vec3 bMultiply(  vec3 a, vec3 b) { return a * b; }
vec3 bScreen(    vec3 a, vec3 b) { return vec3(1.0) - (vec3(1.0) - a) * (vec3(1.0) - b); }
vec3 bOverlay(   vec3 a, vec3 b) {
    vec3 r;
    r.r = a.r < 0.5 ? 2.0 * a.r * b.r : 1.0 - 2.0 * (1.0 - a.r) * (1.0 - b.r);
    r.g = a.g < 0.5 ? 2.0 * a.g * b.g : 1.0 - 2.0 * (1.0 - a.g) * (1.0 - b.g);
    r.b = a.b < 0.5 ? 2.0 * a.b * b.b : 1.0 - 2.0 * (1.0 - a.b) * (1.0 - b.b);
    return r;
}
vec3 bDarken(     vec3 a, vec3 b) { return min(a, b); }
vec3 bLighten(    vec3 a, vec3 b) { return max(a, b); }
vec3 bColorDodge( vec3 a, vec3 b) { return a / (vec3(1.0) - b); }
vec3 bColorBurn(  vec3 a, vec3 b) { return vec3(1.0) - (vec3(1.0) - a) / b; }
vec3 bHardLight(  vec3 a, vec3 b) { return bOverlay(b, a); }
vec3 bSoftLight(  vec3 a, vec3 b) {
    vec3 r;
    r.r = b.r < 0.5 ? a.r * (2.0 * b.r + a.r * (1.0 - 2.0 * b.r)) : a.r + (2.0 * b.r - 1.0) * (sqrt(a.r) - a.r);
    r.g = b.g < 0.5 ? a.g * (2.0 * b.g + a.g * (1.0 - 2.0 * b.g)) : a.g + (2.0 * b.g - 1.0) * (sqrt(a.g) - a.g);
    r.b = b.b < 0.5 ? a.b * (2.0 * b.b + a.b * (1.0 - 2.0 * b.b)) : a.b + (2.0 * b.b - 1.0) * (sqrt(a.b) - a.b);
    return r;
}
vec3 bDifference( vec3 a, vec3 b) { return abs(a - b); }
vec3 bExclusion(  vec3 a, vec3 b) { return a + b - 2.0 * a * b; }
vec3 bHue(        vec3 a, vec3 b) { return setLum(b, lum(a)); }
vec3 bSaturation( vec3 a, vec3 b) { return setLum(setLum(b, lum(a)), lum(a)); }
vec3 bColor(      vec3 a, vec3 b) { return setLum(b, lum(a)); }
vec3 bLuminosity( vec3 a, vec3 b) { return setLum(a, lum(b)); }
vec3 bAdd(        vec3 a, vec3 b) { return a + b; }
vec3 bSubtract(   vec3 a, vec3 b) { return a - b; }
vec3 bDivide(     vec3 a, vec3 b) { return a / b; }
vec3 bLinearBurn( vec3 a, vec3 b) { return a + b - vec3(1.0); }
vec3 bLinearDodge(vec3 a, vec3 b) { return a + b; }
vec3 bVividLight( vec3 a, vec3 b) {
    vec3 r;
    r.r = b.r < 0.5 ? bColorBurn(a.r, 2.0 * b.r) : bColorDodge(a.r, 2.0 * (b.r - 0.5));
    r.g = b.g < 0.5 ? bColorBurn(a.g, 2.0 * b.g) : bColorDodge(a.g, 2.0 * (b.g - 0.5));
    r.b = b.b < 0.5 ? bColorBurn(a.b, 2.0 * b.b) : bColorDodge(a.b, 2.0 * (b.b - 0.5));
    return r;
}
vec3 bLinearLight(vec3 a, vec3 b) { return bLinearDodge(a, 2.0 * b - vec3(1.0)); }
vec3 bPinLight(   vec3 a, vec3 b) {
    vec3 r;
    r.r = b.r < 0.5 ? min(a.r, 2.0 * b.r) : max(a.r, 2.0 * (b.r - 0.5));
    r.g = b.g < 0.5 ? min(a.g, 2.0 * b.g) : max(a.g, 2.0 * (b.g - 0.5));
    r.b = b.b < 0.5 ? min(a.b, 2.0 * b.b) : max(a.b, 2.0 * (b.b - 0.5));
    return r;
}
vec3 bHardMix(    vec3 a, vec3 b) {
    vec3 r;
    r.r = (a.r + b.r) < 1.0 ? 0.0 : 1.0;
    r.g = (a.g + b.g) < 1.0 ? 0.0 : 1.0;
    r.b = (a.b + b.b) < 1.0 ? 0.0 : 1.0;
    return r;
}

void main()
{
    vec4 base  = texture2D(s_baseTex,  v_texcoord0) * u_opacity;
    vec4 blend = texture2D(s_blendTex, v_texcoord0) * u_blendAlpha;
    vec3 c;
    float alpha;

    switch (u_blendMode) {
        case 0:  // Normal (alpha blend)
            c = mix(base.rgb, blend.rgb, blend.a);
            alpha = clamp(base.a + blend.a, 0.0, 1.0);
            break;
        case 1:  /* Multiply   */ c = bMultiply(base.rgb, blend.rgb);   alpha = base.a; break;
        case 2:  /* Screen     */ c = bScreen(base.rgb, blend.rgb);     alpha = base.a; break;
        case 3:  /* Overlay    */ c = bOverlay(base.rgb, blend.rgb);    alpha = base.a; break;
        case 4:  /* Darken     */ c = bDarken(base.rgb, blend.rgb);     alpha = base.a; break;
        case 5:  /* Lighten    */ c = bLighten(base.rgb, blend.rgb);    alpha = base.a; break;
        case 6:  /* ColorDodge */ c = bColorDodge(base.rgb, blend.rgb); alpha = base.a; break;
        case 7:  /* ColorBurn  */ c = bColorBurn(base.rgb, blend.rgb);  alpha = base.a; break;
        case 8:  /* HardLight  */ c = bHardLight(base.rgb, blend.rgb);  alpha = base.a; break;
        case 9:  /* SoftLight  */ c = bSoftLight(base.rgb, blend.rgb);  alpha = base.a; break;
        case 10: /* Difference */ c = bDifference(base.rgb, blend.rgb); alpha = base.a; break;
        case 11: /* Exclusion  */ c = bExclusion(base.rgb, blend.rgb);  alpha = base.a; break;
        case 12: /* Hue        */ c = bHue(base.rgb, blend.rgb);        alpha = base.a; break;
        case 13: /* Saturation */ c = bSaturation(base.rgb, blend.rgb); alpha = base.a; break;
        case 14: /* Color      */ c = bColor(base.rgb, blend.rgb);      alpha = base.a; break;
        case 15: /* Luminosity */ c = bLuminosity(base.rgb, blend.rgb); alpha = base.a; break;
        case 16: /* Add        */ c = bAdd(base.rgb, blend.rgb);        alpha = base.a; break;
        case 17: /* Subtract   */ c = bSubtract(base.rgb, blend.rgb);   alpha = base.a; break;
        case 18: /* Divide     */ c = bDivide(base.rgb, blend.rgb);     alpha = base.a; break;
        case 19: /* LinearBurn */ c = bLinearBurn(base.rgb, blend.rgb); alpha = base.a; break;
        case 20: /* LinearDodge*/ c = bLinearDodge(base.rgb, blend.rgb);alpha = base.a; break;
        case 21: /* VividLight */ c = bVividLight(base.rgb, blend.rgb); alpha = base.a; break;
        case 22: /* LinearLight*/ c = bLinearLight(base.rgb, blend.rgb);alpha = base.a; break;
        case 23: /* PinLight   */ c = bPinLight(base.rgb, blend.rgb);   alpha = base.a; break;
        case 24: /* HardMix    */ c = bHardMix(base.rgb, blend.rgb);    alpha = base.a; break;
        case 25: // Invert
            c = vec3(1.0) - base.rgb;
            alpha = base.a;
            break;
        case 26: // Alpha — just pass the base through
            c = base.rgb;
            alpha = base.a * blend.a;
            break;
        case 27: // Erase — cut out blend region from base
            c = base.rgb;
            alpha = base.a * (1.0 - blend.a);
            break;
        default:
            c = mix(base.rgb, blend.rgb, blend.a);
            alpha = clamp(base.a + blend.a, 0.0, 1.0);
            break;
    }

    gl_FragColor = vec4(c, alpha) * u_globalAlpha;
}