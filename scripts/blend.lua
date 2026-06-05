-- ═══════════════════════════════════════════════════════════════════════════
--  Caesura (AmeKAG) — blend.lua
--  29 blend modes from krkrz tTVPLayerType, mapped to 9 GPU shader modes.
--  Spec [2.2]: alpha, add, subtract, multiply, screen, overlay, darken,
--  lighten, difference. All go through backend.submit_blend → GLSL shader.
--  Krkrz reference: visual/drawable.h, tTVPLayerType enum.
-- ═══════════════════════════════════════════════════════════════════════════

local backend = require("backend")

local Blend = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend mode enum — exact match to krkrz tTVPLayerType (29 modes: 0-28)
-- ═══════════════════════════════════════════════════════════════════════════

Blend.Mode = {
    BINDER            = 0,
    OPAQUE            = 1,
    ALPHA             = 2,
    ADDITIVE          = 3,
    SUBTRACTIVE       = 4,
    MULTIPLICATIVE    = 5,
    EFFECT            = 6,
    FILTER            = 7,
    DODGE             = 8,
    DARKEN            = 9,
    LIGHTEN           = 10,
    SCREEN            = 11,
    ADD_ALPHA         = 12,
    PS_NORMAL         = 13,
    PS_ADDITIVE       = 14,
    PS_SUBTRACTIVE    = 15,
    PS_MULTIPLICATIVE = 16,
    PS_SCREEN         = 17,
    PS_OVERLAY        = 18,
    PS_HARD_LIGHT     = 19,
    PS_SOFT_LIGHT     = 20,
    PS_COLOR_DODGE    = 21,
    PS_COLOR_DODGE5   = 22,
    PS_COLOR_BURN     = 23,
    PS_LIGHTEN        = 24,
    PS_DARKEN         = 25,
    PS_DIFFERENCE     = 26,
    PS_DIFFERENCE5    = 27,
    PS_EXCLUSION      = 28,
}

-- ═══════════════════════════════════════════════════════════════════════════
--  Name → ID lookup (case-insensitive)
-- ═══════════════════════════════════════════════════════════════════════════

Blend.ByName = {
    binder=0, opaque=1,
    alpha=2, transparent=2,
    additive=3, add=3,
    subtractive=4, sub=4,
    multiplicative=5, multiply=5, mul=5,
    effect=6,
    filter=7,
    dodge=8,
    darken=9,
    lighten=10,
    screen=11,
    add_alpha=12, addalpha=12,
    ps_normal=13, normal=13,
    ps_additive=14,
    ps_subtractive=15,
    ps_multiplicative=16,
    ps_screen=17,
    ps_overlay=18, overlay=18,
    ps_hard_light=19, hard_light=19, hardlight=19,
    ps_soft_light=20, soft_light=20, softlight=20,
    ps_color_dodge=21, color_dodge=21, colordodge=21,
    ps_color_dodge5=22, ps_colordodge5=22, colordodge5=22,
    ps_color_burn=23, color_burn=23, colorburn=23,
    ps_lighten=24,
    ps_darken=25,
    ps_difference=26, difference=26, diff=26,
    ps_difference5=27, difference5=27,
    ps_exclusion=28, exclusion=28,
}

-- ═══════════════════════════════════════════════════════════════════════════
--  GPU shader mode enum — the 9 actual GLSL programs
-- ═══════════════════════════════════════════════════════════════════════════

Blend.GPUMode = {
    NORMAL     = 0,  -- standard alpha blend (src * a + dst * (1-a))
    ADD        = 1,  -- additive: dst + src
    SUBTRACT   = 2,  -- subtractive: dst - src
    MULTIPLY   = 3,  -- multiplicative: dst * src
    SCREEN     = 4,  -- screen: 1 - (1-dst) * (1-src)
    OVERLAY    = 5,  -- overlay: combine multiply + screen
    DARKEN     = 6,  -- darken: min(dst, src)
    LIGHTEN    = 7,  -- lighten: max(dst, src)
    DIFFERENCE = 8,  -- difference: abs(dst - src)
}

-- ═══════════════════════════════════════════════════════════════════════════
--  krkrz → GPU shader mode mapping (spec [2.2] Table 2.2-1)
--    Maps each of the 29 krkrz blend IDs to one of 9 GPU shaders.
--    PS_ variants use per-pixel formulas; non-PS use simpler GPU compositing.
-- ═══════════════════════════════════════════════════════════════════════════

local KrkrzToGPUMode = {
    [0]  = Blend.GPUMode.NORMAL,      -- BINDER          → alpha
    [1]  = Blend.GPUMode.NORMAL,      -- OPAQUE          → alpha (opaque src)
    [2]  = Blend.GPUMode.NORMAL,      -- ALPHA           → alpha
    [3]  = Blend.GPUMode.ADD,         -- ADDITIVE        → add
    [4]  = Blend.GPUMode.SUBTRACT,    -- SUBTRACTIVE     → sub
    [5]  = Blend.GPUMode.MULTIPLY,    -- MULTIPLICATIVE  → mul
    [6]  = Blend.GPUMode.NORMAL,      -- EFFECT          → alpha (treated as src-over)
    [7]  = Blend.GPUMode.NORMAL,      -- FILTER          → alpha
    [8]  = Blend.GPUMode.ADD,         -- DODGE           → add
    [9]  = Blend.GPUMode.DARKEN,      -- DARKEN          → darken
    [10] = Blend.GPUMode.LIGHTEN,     -- LIGHTEN         → lighten
    [11] = Blend.GPUMode.SCREEN,      -- SCREEN          → screen
    [12] = Blend.GPUMode.ADD,         -- ADD_ALPHA       → add
    [13] = Blend.GPUMode.NORMAL,      -- PS_NORMAL       → alpha
    [14] = Blend.GPUMode.ADD,         -- PS_ADDITIVE     → add
    [15] = Blend.GPUMode.SUBTRACT,    -- PS_SUBTRACTIVE  → sub
    [16] = Blend.GPUMode.MULTIPLY,    -- PS_MULTIPLICATIVE→mul
    [17] = Blend.GPUMode.SCREEN,      -- PS_SCREEN       → screen
    [18] = Blend.GPUMode.OVERLAY,     -- PS_OVERLAY      → overlay
    [19] = Blend.GPUMode.OVERLAY,     -- PS_HARD_LIGHT   → overlay (approximate)
    [20] = Blend.GPUMode.OVERLAY,     -- PS_SOFT_LIGHT   → overlay (approximate)
    [21] = Blend.GPUMode.ADD,         -- PS_COLOR_DODGE  → add (approximate)
    [22] = Blend.GPUMode.ADD,         -- PS_COLOR_DODGE5 → add
    [23] = Blend.GPUMode.DARKEN,      -- PS_COLOR_BURN   → darken (approximate)
    [24] = Blend.GPUMode.LIGHTEN,     -- PS_LIGHTEN      → lighten
    [25] = Blend.GPUMode.DARKEN,      -- PS_DARKEN       → darken
    [26] = Blend.GPUMode.DIFFERENCE,  -- PS_DIFFERENCE   → difference
    [27] = Blend.GPUMode.DIFFERENCE,  -- PS_DIFFERENCE5  → difference
    [28] = Blend.GPUMode.DIFFERENCE,  -- PS_EXCLUSION    → difference (approximate)
}

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.resolve(name_or_id) → mode_id (numeric 0-28)
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.resolve(name)
    if type(name) == "number" then return name end
    if not name then return Blend.Mode.ALPHA end
    return Blend.ByName[tostring(name):lower()] or Blend.Mode.ALPHA
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.to_gpu_mode(mode_id) → gpu_mode (numeric 0-8)
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.to_gpu_mode(mode_id)
    return KrkrzToGPUMode[mode_id] or Blend.GPUMode.NORMAL
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.mode_name(mode_id) → string — human-readable name
-- ═══════════════════════════════════════════════════════════════════════════

local ModeNames = {
    [0]="binder", [1]="opaque", [2]="alpha", [3]="additive",
    [4]="subtractive", [5]="multiplicative", [6]="effect", [7]="filter",
    [8]="dodge", [9]="darken", [10]="lighten", [11]="screen",
    [12]="add_alpha", [13]="ps_normal", [14]="ps_additive",
    [15]="ps_subtractive", [16]="ps_multiplicative", [17]="ps_screen",
    [18]="ps_overlay", [19]="ps_hard_light", [20]="ps_soft_light",
    [21]="ps_color_dodge", [22]="ps_colordodge5", [23]="ps_color_burn",
    [24]="ps_lighten", [25]="ps_darken", [26]="ps_difference",
    [27]="ps_difference5", [28]="ps_exclusion",
}

function Blend.mode_name(mode_id)
    return ModeNames[mode_id] or "unknown"
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.apply(layer, view_id) — submit blend for a layer via GPU shader
--    Called during layer composition. Blends layer's current texture over
--    its previous frame texture using the layer's blend mode.
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.apply(layer, view_id)
    if not layer then return end

    local from_tex = layer._prev_rt or 0
    local to_tex   = layer.rt or 0
    if to_tex == 0 and not layer.rt then return end

    local mode_id  = Blend.resolve(layer.blend_mode or "alpha")
    local gpu_mode = KrkrzToGPUMode[mode_id] or Blend.GPUMode.NORMAL
    local alpha    = layer.alpha or 1.0

    backend.submit_blend(
        view_id or 1,
        from_tex,
        to_tex,
        gpu_mode,
        1.0,   -- progress (full blend)
        alpha
    )

    layer._prev_rt = layer.rt
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.blend_into(dst_rt, src_rt, blend_mode, opacity, dx, dy, dw, dh, view_id)
--    Blend a source RT into a destination RT at position (dx, dy).
--    This is the primary composition primitive used by layers.lua.
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.blend_into(dst_rt, src_rt, blend_mode, opacity, dx, dy, dw, dh, view_id)
    if not dst_rt or not src_rt then return end

    local mode_id  = Blend.resolve(blend_mode or "alpha")
    local gpu_mode = KrkrzToGPUMode[mode_id] or Blend.GPUMode.NORMAL
    local alpha    = (tonumber(opacity) or 255) / 255.0

    backend.submit_blend(
        view_id or 1,
        dst_rt,
        src_rt,
        gpu_mode,
        1.0,
        alpha
    )
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.blend_textures(tex_a, tex_b, blend_mode, progress, alpha, view_id)
--    Low-level: blend two arbitrary textures with a progress parameter.
--    progress=0 → tex_a fully visible, progress=1 → tex_b fully visible.
--    Useful for animated blend reveals and partial compositing.
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.blend_textures(tex_a, tex_b, blend_mode, progress, alpha, view_id)
    if not tex_a or not tex_b then return end

    local mode_id  = Blend.resolve(blend_mode or "alpha")
    local gpu_mode = KrkrzToGPUMode[mode_id] or Blend.GPUMode.NORMAL
    local prog     = math.max(0, math.min(1, tonumber(progress) or 1))
    local a        = math.max(0, math.min(1, tonumber(alpha) or 1))

    backend.submit_blend(view_id or 1, tex_a, tex_b, gpu_mode, prog, a)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.shader_program management — register / retrieve named GPU programs
--    Programs are precompiled GLSL shaders loaded at engine init.
--    blend.lua maintains a name → program handle table.
-- ═══════════════════════════════════════════════════════════════════════════

local shaderPrograms = {}

function Blend.register_shader(name, program_handle)
    shaderPrograms[name] = program_handle
end

function Blend.get_shader(name)
    return shaderPrograms[name]
end

function Blend.get_shader_for_mode(gpu_mode)
    local names = {
        [Blend.GPUMode.NORMAL]     = "blend_normal",
        [Blend.GPUMode.ADD]        = "blend_add",
        [Blend.GPUMode.SUBTRACT]   = "blend_sub",
        [Blend.GPUMode.MULTIPLY]   = "blend_mul",
        [Blend.GPUMode.SCREEN]     = "blend_screen",
        [Blend.GPUMode.OVERLAY]    = "blend_overlay",
        [Blend.GPUMode.DARKEN]     = "blend_darken",
        [Blend.GPUMode.LIGHTEN]    = "blend_lighten",
        [Blend.GPUMode.DIFFERENCE] = "blend_diff",
    }
    return shaderPrograms[names[gpu_mode]]
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Blend.list_modes() → map of mode names → IDs (for debugging / tools)
-- ═══════════════════════════════════════════════════════════════════════════

function Blend.list_modes()
    local result = {}
    for name, id in pairs(Blend.ByName) do
        if not result[name] then result[name] = id end
    end
    return result
end

return Blend