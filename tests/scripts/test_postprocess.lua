-- test_postprocess.lua — Unit tests for [postprocess] and [postprocess_off]
package.path = "scripts/?.lua;scripts/kag/?.lua;tests/scripts/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond)
    if cond then
        print("PASS " .. name)
        passed = passed + 1
    else
        print("FAIL " .. name)
        failed = failed + 1
    end
end

local schema = require("kag.schema")
local VFXCommands = require("kag.commands.vfx")
local KAG = require("kag")
local backend = require("backend")

-- ═══════════════════════════════════════════════════════════════════════
-- 1. Schema Registration & Coercion Tests
-- ═══════════════════════════════════════════════════════════════════════
check("postprocess schema is registered", schema.isMigrated("postprocess"))
check("postprocess_off schema is registered", schema.isMigrated("postprocess_off"))

-- Missing required param 'effect'
local ok, err = pcall(function()
    schema.coerce("postprocess", {}, {})
end)
check("postprocess requires effect", not ok)

-- Invalid effect value throws
ok, err = pcall(function()
    schema.coerce("postprocess", { effect = "invalid_effect" }, {})
end)
check("postprocess invalid effect throws", not ok)

-- Valid effect enums
for _, eff in ipairs({ "bloom", "vignette", "lut", "softblur", "off", "none" }) do
    local p = schema.coerce("postprocess", { effect = eff }, {})
    check("postprocess accepts effect=" .. eff, p.effect == eff)
end

-- Default values
local defParams = schema.coerce("postprocess", { effect = "bloom" }, {})
-- intensity/strength are aliases and neither carries a schema default: a
-- default on one makes the other unreachable (the handler cannot tell
-- "omitted" from "schema-supplied"). The 1.0 fallback lives in apply_postfx.
check("intensity has no schema default (strength= stays reachable)",
      defParams.intensity == nil)
check("default radius is 0.0", defParams.radius == 0.0)
check("default amount is 0.0", defParams.amount == 0.0)
-- lutMix default is 0, deliberately: it must match the single fallback in
-- vfx.lua apply_postfx, the legacy [vfx postfx=lut] path and the C++ default
-- (RenderBinding.cpp resolvePostFxParams, pinned by test_render_postfx.cpp:60).
check("default lutMix is 0.0 (parity with [vfx] and C++)", defParams.lutMix == 0.0)
check("default r is 255", defParams.r == 255)
check("default g is 255", defParams.g == 255)
check("default b is 255", defParams.b == 255)

-- Clamping tests
local clamped = schema.coerce("postprocess", {
    effect = "bloom",
    intensity = "2.5",
    radius = "100",
    amount = "5.0",
    lutMix = "3.0",
    r = "300",
    g = "-10",
    b = "500",
}, {})
check("intensity clamped to 1.0", clamped.intensity == 1.0)
check("radius clamped to 64.0", clamped.radius == 64.0)
check("amount clamped to 1.0", clamped.amount == 1.0)
check("lutMix clamped to 1.0", clamped.lutMix == 1.0)
check("r clamped to 255", clamped.r == 255)
check("g clamped to 0", clamped.g == 0)
check("b clamped to 255", clamped.b == 255)

local minClamped = schema.coerce("postprocess", {
    effect = "bloom",
    intensity = "-1.0",
    radius = "-5.0",
    amount = "-0.5",
    lutMix = "-2.0",
}, {})
check("intensity min clamped to 0.0", minClamped.intensity == 0.0)
check("radius min clamped to 0.0", minClamped.radius == 0.0)
check("amount min clamped to 0.0", minClamped.amount == 0.0)
check("lutMix min clamped to 0.0", minClamped.lutMix == 0.0)

-- ═══════════════════════════════════════════════════════════════════════
-- 2. Mock Backend & Command Handler Execution Tests
-- ═══════════════════════════════════════════════════════════════════════
local postfxCalls = {}
local supportedMap = {
    bloom = true,
    vignette = true,
    lut = true,
    softblur = true,
}

local orig_set_postfx = backend.set_postfx
local orig_clear_postfx = backend.clear_postfx
local orig_destroy_postfx = backend.destroy_postfx
local orig_is_postfx_supported = backend.is_postfx_supported
local orig_is_postfx_active = backend.is_postfx_active

backend.set_postfx = function(kind, params)
    postfxCalls[#postfxCalls + 1] = { action = "set", kind = kind, params = params }
    return 101 -- mock handle
end
backend.clear_postfx = function()
    postfxCalls[#postfxCalls + 1] = { action = "clear" }
    return true
end
backend.destroy_postfx = function(kind)
    postfxCalls[#postfxCalls + 1] = { action = "destroy", kind = kind }
    return true
end
backend.is_postfx_supported = function(kind)
    return supportedMap[kind] == true
end
backend.is_postfx_active = function()
    return true
end

-- Test bloom effect execution
do
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "bloom", intensity = 0.85, radius = 8.0 }, ctx)
    local handle = VFXCommands.postprocess(ctx, p)
    check("postprocess bloom returns handle", handle == 101)
    check("postprocess bloom logged set call", #postfxCalls == 1 and postfxCalls[1].kind == "bloom")
    check("postprocess bloom passed strength", postfxCalls[1].params.strength == 0.85)
    check("postprocess bloom passed radius", postfxCalls[1].params.radius == 8.0)
    check("postprocess bloom stored in ctx._postfx", ctx._postfx and ctx._postfx.bloom and ctx._postfx.bloom.handle == 101)
end

-- Test vignette effect with explicit r, g, b
do
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "vignette", intensity = 0.6, radius = 24.0, r = 0, g = 0, b = 0 }, ctx)
    local handle = VFXCommands.postprocess(ctx, p)
    check("postprocess vignette handle returned", handle == 101)
    check("postprocess vignette logged set call", #postfxCalls == 1 and postfxCalls[1].kind == "vignette")
    check("postprocess vignette formatted rgb", postfxCalls[1].params.rgb == "0,0,0")
    check("postprocess vignette radius passed", postfxCalls[1].params.radius == 24.0)
end

-- Test lut color grading with lutMix
do
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "lut", lutMix = 0.75 }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("postprocess lut logged set call", #postfxCalls == 1 and postfxCalls[1].kind == "lut")
    check("postprocess lut lutMix passed", postfxCalls[1].params.lutMix == 0.75)
end

-- Test softblur with radius and amount
do
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "softblur", radius = 16.0, amount = 0.5 }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("postprocess softblur logged set call", #postfxCalls == 1 and postfxCalls[1].kind == "softblur")
    check("postprocess softblur radius passed", postfxCalls[1].params.radius == 16.0)
    check("postprocess softblur amount passed", postfxCalls[1].params.amount == 0.5)
end

-- Test effect="off" and effect="none" clears postfx
do
    postfxCalls = {}
    local ctx = { _postfx = { bloom = { handle = 101 } } }
    local p = schema.coerce("postprocess", { effect = "off" }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("postprocess effect=off calls clear_postfx", #postfxCalls == 1 and postfxCalls[1].action == "clear")
    check("postprocess effect=off clears ctx._postfx", ctx._postfx == nil)

    postfxCalls = {}
    ctx._postfx = { vignette = { handle = 102 } }
    p = schema.coerce("postprocess", { effect = "none" }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("postprocess effect=none calls clear_postfx", #postfxCalls == 1 and postfxCalls[1].action == "clear")
    check("postprocess effect=none clears ctx._postfx", ctx._postfx == nil)
end

-- Test postprocess_off command
do
    postfxCalls = {}
    local ctx = { _postfx = { bloom = { handle = 101 } } }
    local p = schema.coerce("postprocess_off", {}, ctx)
    VFXCommands.postprocess_off(ctx, p)
    check("postprocess_off calls clear_postfx", #postfxCalls == 1 and postfxCalls[1].action == "clear")
    check("postprocess_off clears ctx._postfx", ctx._postfx == nil)
end

-- Test graceful degradation on unsupported device
do
    supportedMap.bloom = false
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "bloom" }, ctx)
    local ret = VFXCommands.postprocess(ctx, p)
    check("unsupported effect does not call set_postfx", #postfxCalls == 0)
    check("unsupported effect returns nil/safe", ret == nil)
    supportedMap.bloom = true
end

-- ═══════════════════════════════════════════════════════════════════════
-- 2b. ONE implementation, two entry points (convergence regression)
-- ═══════════════════════════════════════════════════════════════════════
--  [vfx postfx=x] and [postprocess effect=x] used to be two near-identical
--  copies of the same logic that DISAGREED on the lutMix fallback (0 vs 1.0):
--  the same effect graded fully through one command and not at all through the
--  other. They now share apply_postfx, and these cases pin that: given the
--  same author-supplied values both entries must produce the same
--  PostFxParams table.
do
    postfxCalls = {}
    local ctxA, ctxB = {}, {}
    -- [vfx postfx="lut" strength=0.5]  (validation-only schema: no defaults)
    VFXCommands._postfx(ctxA, { postfx = "lut", strength = 0.5 })
    local viaVfx = postfxCalls[1] and postfxCalls[1].params

    postfxCalls = {}
    -- [postprocess effect="lut" strength=0.5] through the real coercion
    local p = schema.coerce("postprocess", { effect = "lut", strength = 0.5 }, ctxB)
    VFXCommands.postprocess(ctxB, p)
    local viaPP = postfxCalls[1] and postfxCalls[1].params

    check("both entries reach set_postfx", viaVfx ~= nil and viaPP ~= nil)
    if viaVfx and viaPP then
        check("strength identical across entries", viaVfx.strength == viaPP.strength)
        check("lutMix identical across entries (was 0 vs 1.0)",
              viaVfx.lutMix == viaPP.lutMix)
        check("radius identical across entries", viaVfx.radius == viaPP.radius)
        check("amount identical across entries", viaVfx.amount == viaPP.amount)
    end
end

do
    -- strength= must not be shadowed by the intensity default. Before the
    -- convergence the handler read `params.intensity or params.strength`, and
    -- since the schema always fills intensity=1.0, an author writing only
    -- strength=0.25 silently got 1.0.
    postfxCalls = {}
    local ctx = {}
    local p = schema.coerce("postprocess", { effect = "bloom", strength = 0.25 }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("strength= alone is honoured (not shadowed by intensity default)",
          postfxCalls[1] and postfxCalls[1].params.strength == 0.25)

    -- When both are given the declared name (intensity) wins, deterministically.
    postfxCalls = {}
    p = schema.coerce("postprocess", { effect = "bloom", intensity = 0.9, strength = 0.1 }, ctx)
    VFXCommands.postprocess(ctx, p)
    check("intensity wins when both intensity and strength are given",
          postfxCalls[1] and postfxCalls[1].params.strength == 0.9)
end

do
    -- [postprocess_off effect=x] tears down ONE stage; bare form clears all.
    postfxCalls = {}
    local ctx = { _postfx = { bloom = { handle = 101 }, vignette = { handle = 102 } } }
    local p = schema.coerce("postprocess_off", { effect = "vignette" }, ctx)
    VFXCommands.postprocess_off(ctx, p)
    check("postprocess_off effect= calls destroy_postfx",
          #postfxCalls == 1 and postfxCalls[1].action == "destroy"
          and postfxCalls[1].kind == "vignette")
    check("postprocess_off effect= drops only that stage from ctx._postfx",
          ctx._postfx and ctx._postfx.vignette == nil and ctx._postfx.bloom ~= nil)

    -- Removing the last stage collapses ctx._postfx back to nil (so the
    -- "nothing active" state has one representation, not two).
    postfxCalls = {}
    p = schema.coerce("postprocess_off", { effect = "bloom" }, ctx)
    VFXCommands.postprocess_off(ctx, p)
    check("removing the last stage clears ctx._postfx entirely", ctx._postfx == nil)

    postfxCalls = {}
    ctx._postfx = { bloom = { handle = 101 } }
    p = schema.coerce("postprocess_off", { effect = "all" }, ctx)
    VFXCommands.postprocess_off(ctx, p)
    check('postprocess_off effect="all" clears the whole chain',
          #postfxCalls == 1 and postfxCalls[1].action == "clear"
          and ctx._postfx == nil)
end

-- ═══════════════════════════════════════════════════════════════════════
-- 3. KAG Table & Integration Tests
-- ═══════════════════════════════════════════════════════════════════════
check("KAG.postprocess is bound", type(KAG.postprocess) == "function")
check("KAG.postprocess_off is bound", type(KAG.postprocess_off) == "function")

do
    postfxCalls = {}
    local ctx = {}
    KAG.postprocess(ctx, { effect = "bloom", intensity = 0.5 })
    check("KAG.postprocess dispatches to backend", #postfxCalls == 1 and postfxCalls[1].kind == "bloom")

    postfxCalls = {}
    KAG.postprocess_off(ctx, {})
    check("KAG.postprocess_off dispatches to clear", #postfxCalls == 1 and postfxCalls[1].action == "clear")
end

-- Restore backend
backend.set_postfx = orig_set_postfx
backend.clear_postfx = orig_clear_postfx
backend.destroy_postfx = orig_destroy_postfx
backend.is_postfx_supported = orig_is_postfx_supported
backend.is_postfx_active = orig_is_postfx_active

if failed > 0 then
    print(string.format("TESTS FAILED: %d failures", failed))
    os.exit(1)
end
print("POSTPROCESS TESTS DONE")
