-- test_font.lua — [font] family contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- self-sufficient stub: the suite relies on test_macro setting
-- _G.Render.text_set_font -- standalone runs must not depend on it.
-- pcall: the sandbox forbids modifying Render in the suite, where the
-- stub already exists anyway.
-- pcall the whole probe: the sandbox BLOCKS reads of unknown Render
-- fields ("blocked in strict mode"), and the suite-wide stub already
-- exists (test_kag_commands sets it pre-sandbox) -- standalone runs
-- install it here.
local probe_ok, stub_exists = pcall(function()
    return type(_G.Render and _G.Render.text_set_font) == "function"
end)
-- pcall's FIRST return is the success flag; the probe's own return is
-- the existence flag (audit: has_stub = pcall(...) was always true,
-- so standalone runs skipped the stub install and font crashed)
if not (probe_ok and stub_exists) then
    pcall(function()
        if _G.Render == nil then _G.Render = {} end
        _G.Render.text_set_font = function() end
    end)
end

local Text = package.loaded["kag.commands.text"] or require("kag.commands.text")
local KAG = require("kag")
check("font handler", type(KAG.font) == "function")

-- schema: size clamped 4..256
local schema = require("kag.schema")
local c1 = schema.coerce("font", { size = "999" })
check("size clamped", c1.size == 256)
local c2 = schema.coerce("font", { size = "1" })
check("size min clamped", c2.size == 4)
check("face default", c2.face == "default")

-- behavior: state persists into ctx.text_state (no backend dependency
-- in the state write -- backend.text_set_font resolves to nil safely
-- via the resolve chain in test env)
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
local ok = pcall(KAG.font, ctx, { face = "serif", size = 30, color = "255,0,0" })
check("font pcall ok", ok)
check("face persisted", ctx.text_state.font_face == "serif")
check("size persisted", ctx.text_state.font_size == 30)
check("color persisted", ctx.text_state.font_color == "255,0,0")

-- idempotent empty font (no params) keeps state, no crash
local ok2 = pcall(KAG.font, ctx, {})
check("empty font no crash", ok2)
check("state kept", ctx.text_state.font_size == 30)

-- [skip]/[auto] toggles still registered
check("skip registered", type(KAG.skip) == "function")
check("auto registered", type(KAG.auto) == "function")

if failed > 0 then os.exit(1) end
print("FONT TESTS DONE")
