-- test_hr.lua -- [hr] horizontal-rule wiring lock (t202)
-- Locks the WIRED semantics of the KAG3-compat [hr]: the handler at
-- kag.lua:217 delegates to TextCommands.hr (text.lua), which draws a thin
-- full-width rule on the _hr layer at the current text cursor, advances
-- the cursor one row, and is hidden by [ch]/[text]/[er]/[p]/[reset].
-- The old no-op era is replaced: parse + no-crash compatibility still
-- hold (test_four_remaining's assertions keep passing), and the draw /
-- cleanup behavior is now asserted. Standalone harness (module-backend
-- stub mirrors the suite seam; rtt mocked since the engine is absent).
package.path = "scripts/?.lua;scripts/?/init.lua;scripts/kag/?.lua;scripts/kag/commands/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local texN = 0
package.loaded["backend"] = {
    get_resolution = function() return 1920, 1080 end,
    line_height = function() return 32 end,
    create_solid_texture = function(r, g, b, a) texN = texN + 1; return texN end,
    clear_text = function() end,
    text_reset_state = function() end,
}
package.loaded["rtt"] = { acquire = function() return 0 end, release = function() end }

local KAG = require("kag")
local TextCmds = require("kag.commands.text")
local tokenizer = require("tokenizer")
local layers = require("layers")

local function mkctx()
    return { f = {}, tf = {}, sf = {}, mp = {}, variables = {}, backlog = {},
             textCursorY = 580, textCursorX = 32,
             current_scene = "probe.ks", currentScene = "probe.ks" }
end

-- 1. Dispatch identity: KAG.hr delegate is live, not the old no-op
check("TextCommands.hr implemented", type(TextCmds.hr) == "function")
check("KAG.hr registered", type(KAG.hr) == "function")
do
    local i1 = debug.getinfo(KAG.hr)
    local i2 = debug.getinfo(TextCmds.hr)
    print("  [evidence] KAG.hr at " .. tostring(i1.short_src) .. ":" .. tostring(i1.linedefined)
          .. " -> TextCmds.hr at " .. tostring(i2.short_src) .. ":" .. tostring(i2.linedefined))
    check("dispatch points to real implementation",
          i1.short_src:match("kag%.lua$") and i2.short_src:match("text%.lua$") and i2.linedefined > 850)
end

-- 2. No-crash on bare ctx (test_four_remaining compatibility: the old
--    'hr no-op safe' assertion must still hold with the real handler)
do
    local ok = pcall(KAG.hr, { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }, {})
    check("hr no-crash on bare ctx", ok)
end

-- 3. Parse shape
check("hr parses", tokenizer.parse("[hr]")[1].cmd == "hr")

-- 4. Draw trigger: _hr layer created with a solid texture bar at cursor
do
    local ctx = mkctx()
    local ok = pcall(KAG.hr, ctx, {})
    local hr = layers.get("_hr")
    check("hr runs ok", ok)
    check("_hr layer created", hr ~= nil)
    check("_hr visible", hr and hr.visible == true)
    check("_hr solid texture set", hr and hr.texture ~= nil and hr.texture ~= false)
    check("_hr position at cursor (32,580)", hr and hr.x == 32 and hr.y == 580)
    check("_hr full-width bar (1856x2)", hr and hr.w == 1920 - 64 and hr.h == 2)
end

-- 5. Cursor advance: one row down, x reset
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    check("cursor advances one row (580->612)", ctx.textCursorY == 612)
    check("cursor x resets to 32", ctx.textCursorX == 32)
end

-- 6. Reuse: repeated [hr] reuses the same layer node (no leak)
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local first = layers.get("_hr")
    local cnt1 = layers.count()
    pcall(KAG.hr, ctx, {})
    check("reuse same _hr node", layers.get("_hr") == first)
    check("layer count stable", layers.count() == cnt1)
end

-- 7. [ch] hides the standing rule (page finished)
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local ok = pcall(KAG.ch, ctx, { text = "Hi" })
    check("[ch] runs ok", ok)
    check("[ch] hides _hr", layers.get("_hr") and layers.get("_hr").visible == false)
end

-- 8. [text] hides the standing rule
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local ok = pcall(KAG.text, ctx, { text = "Hello" })
    check("[text] runs ok", ok)
    check("[text] hides _hr", layers.get("_hr") and layers.get("_hr").visible == false)
end

-- 9. [er] hides the standing rule
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local ok = pcall(KAG.er, ctx, {})
    check("[er] runs ok", ok)
    check("[er] hides _hr", layers.get("_hr") and layers.get("_hr").visible == false)
end

-- 10. [p] keeps the page (and the rule) visible while waiting, then hides
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local co = coroutine.create(function() KAG.p(ctx, {}) end)
    local ok1 = coroutine.resume(co)
    local during = layers.get("_hr") and layers.get("_hr").visible
    local ok2 = coroutine.resume(co)
    check("[p] first resume ok", ok1)
    check("[p] rule visible during wait", during == true)
    check("[p] second resume ok", ok2)
    check("[p] hides _hr after page break", layers.get("_hr") and layers.get("_hr").visible == false)
end

-- 11. [reset] hides the standing rule
do
    local ctx = mkctx()
    pcall(KAG.hr, ctx, {})
    local ok = pcall(KAG.reset, ctx, {})
    check("[reset] runs ok", ok)
    check("[reset] hides _hr", layers.get("_hr") and layers.get("_hr").visible == false)
end

if failed > 0 then os.exit(1) end
print("HR TESTS DONE (" .. passed .. " passed)")
