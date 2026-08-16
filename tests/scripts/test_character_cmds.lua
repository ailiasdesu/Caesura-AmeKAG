-- test_character_cmds.lua — [csp]/[csd]/[csl] contract + behavior tests.
-- Standalone: external/lua/lua.exe tests/scripts/test_character_cmds.lua
-- Driver mirrors test_layer_cmds.lua: a backend mock records load_texture /
-- render calls while the REAL layers tree is inspected for node state.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- ── Backend mock: records every render()/platform() call, returns tex ids.
local loaded_files = {}
local calls = {}
-- NOTE: this vendored lua treats `t.render(...)` as a FLAT call
-- (no self-binding) -- the real _CAESURA_BACKEND.render(method, ...) receives
-- the method name as its first arg. The mock mirrors that contract.
_G._CAESURA_BACKEND = {
    calls = calls,
    render = function(method, ...)
        local n = select("#", ...)
        local args = {}
        for i = 1, n do args[i] = select(i, ...) end
        calls[#calls + 1] = { method = method, args = args, n = n }
        if method == "load_texture" then
            loaded_files[#loaded_files + 1] = args[1]
            return 1000 + #calls  -- distinct texture id
        end
        return 1
    end,
    platform = function() return false end,
}

-- ── Real layers tree, reset per run.
local layers = require("layers")
layers.init()

local schema = require("kag.schema")
local Char = require("kag.commands.character")

-- ── Schema contracts registered with correct metadata.
check("csp schema migrated", schema.isMigrated("csp"))
check("csd schema migrated", schema.isMigrated("csd"))
check("csl schema migrated", schema.isMigrated("csl"))
do
    local meta = schema.meta("csp")
    check("csp category=layer", meta and meta.category == "layer")
    check("csp blocking=false", meta and meta.blocking == false)
end
check("csp handler", type(Char.csp) == "function")
check("csd handler", type(Char.csd) == "function")
check("csl handler", type(Char.csl) == "function")

-- Helper: fetch a layer node by name.
local function node(name) return layers.get(name) end

-- ── [csp name=hero layer=0 x=320 y=240] — build layer + set image + visible.
do
    local ctx = {}
    local params = schema.coerce("csp", { name = "hero", layer = 0, x = "320", y = "240" }, ctx)
    Char.csp(ctx, params)
    local n = node("0")
    check("csp layer '0' created", n ~= nil)
    if n then
        check("csp layer visible", n.visible == true)
        check("csp image set (tex ~= nil)", n.tex ~= nil)
        check("csp x=320", n.x == 320)
        check("csp y=240", n.y == 240)
    end
    check("csp defaults resolve to assets/char/hero.png",
        loaded_files[#loaded_files] == "assets/char/hero.png")
    check("csp records ctx.layers[0]", ctx.layers and ctx.layers["0"] == "assets/char/hero.png")
    check("csp records ctx.characters[0]",
        ctx.characters and ctx.characters["0"] and ctx.characters["0"].chara == "hero")
end

-- ── Default parameters: bare name only → layer "0", x/y=0.
do
    local ctx = {}
    local params = schema.coerce("csp", { name = "villain" }, ctx)  -- layer/x/y default
    Char.csp(ctx, params)
    local n = node("0")
    check("csp default layer no-op on existing '0'", n and n.visible == true)
    check("csp defaults x=0",
        loaded_files[#loaded_files] == "assets/char/villain.png")
    check("csp coerce fills default layer",
        params.layer == "0" and params.x == 0 and params.y == 0)
end

-- ── Bare positional name [csp hero]: schema positional_index accepts it.
do
    local ctx = {}
    local params = schema.coerce("csp", { "hero" }, ctx)
    Char.csp(ctx, params)
    check("csp positional name filled (round 97: coerced into out.name)",
        params.name == "hero" and params[1] == "hero")
    check("csp positional path uses assets/char/hero.png",
        loaded_files[#loaded_files] == "assets/char/hero.png")
end

-- ── Explicit storage override mirrors [image] path resolution.
do
    local ctx = {}
    local params = schema.coerce("csp", { name = "hero", storage = "chara/special.png" }, ctx)
    Char.csp(ctx, params)
    check("csp storage override wins",
        loaded_files[#loaded_files] == "chara/special.png")
end

-- ── Re-[csp] of the SAME layer with a different chara updates the image.
do
    local ctx = {}
    Char.csp(ctx, schema.coerce("csp", { name = "hero", layer = 0 }, ctx))
    local texBefore = node("0").tex
    Char.csp(ctx, schema.coerce("csp", { name = "hero2", layer = 0 }, ctx))
    local texAfter = node("0").tex
    check("csp re-show updates image (tex changed)",
        texBefore ~= texAfter and node("0").visible == true)
    check("csp re-show records new file",
        ctx.layers["0"] == "assets/char/hero2.png")
end

-- ── [csl name=hero layer=0 x=340 y=240] — move coords, visibility untouched.
do
    local ctx = {}
    local params = schema.coerce("csp", { name = "hero", layer = 0, x = 10, y = 10 }, ctx)
    Char.csp(ctx, params)
    local mparams = schema.coerce("csl", { name = "hero", layer = 0, x = 340, y = 240 }, ctx)
    Char.csl(ctx, mparams)
    local n = node("0")
    check("csl moves x", n.x == 340)
    check("csl moves y", n.y == 240)
    check("csl keeps visible", n.visible == true)
end

-- ── Layer as a STRING name ("charaA") — both visibility and move work.
do
    local ctx = {}
    Char.csp(ctx, schema.coerce("csp", { name = "hero", layer = "charaA", x = 1, y = 2 }, ctx))
    local n = node("charaA")
    check("csp string layer created", n ~= nil)
    check("csp string layer visible", n and n.visible == true)
    Char.csl(ctx, schema.coerce("csl", { name = "hero", layer = "charaA", x = 55, y = 66 }, ctx))
    check("csl string layer moves", n and n.x == 55 and n.y == 66)
end

-- ── [csd name=hero layer=0] — hides the layer and drops dedup state.
do
    local ctx = {}
    Char.csp(ctx, schema.coerce("csp", { name = "hero", layer = 0 }, ctx))
    Char.csd(ctx, schema.coerce("csd", { name = "hero", layer = 0 }, ctx))
    local n = node("0")
    check("csd hides layer", n and n.visible == false)
    check("csd drops texture", n and n.tex == nil)
    check("csd clears ctx.layers", ctx.layers and ctx.layers["0"] == nil)
    check("csd clears ctx.characters", ctx.characters and ctx.characters["0"] == nil)
end

-- ── csd on a string layer name.
do
    local ctx = {}
    Char.csp(ctx, schema.coerce("csp", { name = "hero", layer = "b", x = 1, y = 1 }, ctx))
    Char.csd(ctx, schema.coerce("csd", { name = "hero", layer = "b" }, ctx))
    local n = node("b")
    check("csd string layer hides", n and n.visible == false)
end

-- ── [csl] on a not-yet-shown layer is a safe no-op (no visibility change, no error).
do
    local ctx = {}
    local okCall = pcall(Char.csl, ctx, schema.coerce("csl", { name = "ghost", layer = "zzz" }, ctx))
    check("csl missing layer no-op (pcall)", okCall)
    check("csl missing layer did not create", node("zzz") == nil)
end

-- ── backend mock actually recorded load_texture calls (contract lock).
check("backend.load_texture called", #loaded_files > 0)

if failed > 0 then os.exit(1) end
print("CHARACTER CMDS TESTS DONE")