-- =============================================================================
--  Caesura (AmeKAG) — test_live2d_cmds.lua
--  Unit tests for Live2D motion, expression, and lip-sync command bindings
-- =============================================================================

local BS = string.char(92)
local here = (arg and arg[0] and arg[0]:match("(.*[/" .. BS .. "])")) or "scripts/"
if not package.path:find(here, 1, true) then
    package.path = here .. "?.lua;" .. here .. "?/init.lua;" .. "scripts/?.lua;scripts/?/init.lua;" .. package.path
end

local schema = require("kag.schema")
local charCmds = require("kag.commands.character")

local passed = 0
local failed = 0

local function check(cond, msg)
    if cond then
        passed = passed + 1
        print("  [PASS] " .. msg)
    else
        failed = failed + 1
        print("  [FAIL] " .. msg)
    end
end

print("=== Running Live2D Command Tests ===")

-- 1. Schema Registration
check(schema.specs("live2d_motion") ~= nil, "live2d_motion registered in schema")
check(schema.specs("live2d_expression") ~= nil, "live2d_expression registered in schema")
check(schema.specs("live2d_lip_sync") ~= nil, "live2d_lip_sync registered in schema")

-- 2. Clamping and validation
local ctx = {}
local val_motion = schema.coerce("live2d_motion", { model = "aoi", motion = "wave", fadein = 15000, fadeout = -100 }, ctx, 1)
check(val_motion.fadein == 10000, "fadein clamped to max 10000")
check(val_motion.fadeout == 0, "fadeout clamped to min 0")

charCmds.live2d_motion(ctx, val_motion)
check(ctx.live2d["aoi"].current_motion == "wave", "Motion state updated in ctx")
check(ctx.live2d["aoi"].fadein == 10000, "Fadein stored in ctx")

-- 3. Expression
local val_expr = schema.coerce("live2d_expression", { model = "aoi", expression = "smile", weight = 1.5 }, ctx, 2)
check(val_expr.weight == 1.0, "Expression weight clamped to 1.0")
charCmds.live2d_expression(ctx, val_expr)
check(ctx.live2d["aoi"].expression == "smile", "Expression state updated")
check(ctx.live2d["aoi"].expression_weight == 1.0, "Expression weight stored")

-- 4. Lip Sync
local val_lip = schema.coerce("live2d_lip_sync", { model = "aoi", value = 0.85 }, ctx, 3)
check(val_lip.value == 0.85, "Lip sync value valid")
charCmds.live2d_lip_sync(ctx, val_lip)
check(ctx.live2d["aoi"].lip_sync == 0.85, "Lip sync stored in ctx")

print(string.format("\nLive2D Command Tests: %d passed, %d failed.", passed, failed))
if failed > 0 then os.exit(1) end
