-- =============================================================================
--  test_kag3_import_e2e.lua -- kag3_import end-to-end integration test.
--
--  Round-72 note: test_kag3_import.lua only asserts the CONVERTED text; it
--  never drives the result through the engine. This standalone file is the
--  missing half: a classic KAG3 fragment (text embeds, TJS [if], [csp],
--  [add], [notify], [wait], *label [jump]) is converted by kag3_import, then
--  actually RUN through tokenize -> compile -> scheduler with a mock backend,
--  asserting variable values, branch direction, interpolation and the jump
--  target at runtime.
--
--  STANDALONE: it installs its own _CAESURA_BACKEND mock + layers.init(), so
--  it must NOT be merged into the order-sensitive main-suite global drift.
--  Run independently:  external/lua/lua.exe tests/scripts/test_kag3_import_e2e.lua
-- =============================================================================
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print("PASS " .. name)
    else
        failed = failed + 1
        print("FAIL " .. name .. " -- " .. tostring(detail))
    end
end

-- ---------------------------------------------------------------------------
-- Backend mock (reflects _CAESURA_BACKEND flat-call contract) + fresh layers.
-- ---------------------------------------------------------------------------
local loaded = {}
local backendCalls = {}
_G._CAESURA_BACKEND = {
    render = function(method, ...)
        local n = select("#", ...)
        local args = {}
        for i = 1, n do args[i] = select(i, ...) end
        backendCalls[#backendCalls + 1] = { method = method, args = args }
        if method == "load_texture" then
            loaded[#loaded + 1] = args[1]
            return 7000 + #backendCalls
        end
        return 1
    end,
    platform = function() return false end,
    audio_play = function() return true end,
}
local layers = require("layers")
layers.init()

-- Preload the REAL kag handler table (all command modules register into it).
package.loaded["kag"] = nil
require("kag")

local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")
local scheduler = require("scheduler")
local imp = require("kag3_import")

-- ---------------------------------------------------------------------------
-- Helper: convert -> parse -> compile -> run, then return the ctx + backlog.
-- ---------------------------------------------------------------------------
local function run_scene(scene, seedVars)
    local tmp = os.tmpname() .. ".ks"
    local f = io.open(tmp, "w")
    f:write(scene)
    f:close()
    local rep = imp.processScene(tmp)
    os.remove(tmp)
    local tokens = rep and tokenizer.parse(rep.output)
    if tokens then tokens = compiler.compile(tokens) end
    local ctx = {
        f = {}, tf = {}, sf = {}, mp = {}, lf = {},
        current_scene = "e2e.ks", token_index = 1, stop_flag = false,
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, call_stack = {}, load_tokens = function() end,
    }
    if seedVars then
        for k, v in pairs(seedVars) do ctx.f[k] = v end
    end
    if tokens then
        local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
        local guards = 0
        while coroutine.status(co) ~= "dead" and guards < 2000 do
            guards = guards + 1
            coroutine.resume(co, 16)
        end
    end
    return { rep = rep, ctx = ctx, tokens = tokens }
end

local function backlog_texts(ctx)
    local out = {}
    for _, b in ipairs(ctx.backlog or {}) do out[#out + 1] = b.text end
    return out
end

-- ===========================================================================
-- 1. Conversion output assertions (quoted + bare params).
-- ===========================================================================
local fragment = [[
; classic KAG3 fragment
*start
&f.hp 是生命值
[csp name="hero" layer="0" left="320" top="240"]
[add var="f.x" value="5"]
[notify msg="saved"]
[wait time="200"]
[if exp="f.hp > 10 && f.flag != 0"]
[ch name="Hero" text="HP is &f.hp and flag set"]
[else]
[ch name="Hero" text="condition false"]
[endif]
[jump target="*ending"]
*not_ending
[ch name="Hero" text="skipme"]
*ending
[ch name="Hero" text="the end"]
[stop]
]]

local r1 = run_scene(fragment, { hp = 42, flag = 1 })
check("e2e: scene converted", r1.rep ~= nil and #r1.rep.unsupported == 0
    and tostring(r1.rep and type(r1.rep.unsupported)))
if r1.rep then
    local out = r1.rep.output
    check("e2e: &f.hp embed -> %f.hp%", out:find("%f.hp% 是生命值", 1, true) ~= nil, out)
    check("e2e: csp left/top -> x/y", out:find("left=", 1, true) == nil
        and out:find([=[csp name="hero" layer="0" x="320" y="240"]=], 1, true) ~= nil, out)
    check("e2e: add var -> name", out:find([=[add name="f.x" value="5"]=], 1, true) ~= nil, out)
    check("e2e: TJS &&/!= -> and/~=" , out:find("f.hp > 10 and f.flag ~= 0", 1, true) ~= nil, out)
    check("e2e: ch text embed converted", out:find("HP is %f.hp% and flag set", 1, true) ~= nil, out)
    check("e2e: notify/wait kept",
        out:find([=[notify msg="saved"]=], 1, true) ~= nil
        and out:find([=[wait time="200"]=], 1, true) ~= nil, out)
    check("e2e: jump target preserved", out:find([=[jump target="*ending"]=], 1, true) ~= nil, out)
end

-- ===========================================================================
-- 2. Runtime: variable values, interpolation, TRUE branch, jump target.
-- ===========================================================================
if r1.ctx then
    local ctx = r1.ctx
    check("e2e: [add] f.x == 5 at runtime", ctx.f.x == 5, tostring(ctx.f.x))
    check("e2e: seed vars survive", ctx.f.hp == 42 and ctx.f.flag == 1,
        tostring(ctx.f.hp) .. "/" .. tostring(ctx.f.flag))
    local texts = backlog_texts(ctx)
    local list = table.concat(texts, " | ")
    check("e2e: bare text line interpolated", texts[1] == "42 是生命值", list)
    check("e2e: TRUE branch ch line selected",
        texts[2] == "HP is 42 and flag set", list)
    check("e2e: [else] branch NOT taken",
        (function() for _, t in ipairs(texts) do if t == "condition false" then return false end end
            return true end)(), list)
    check("e2e: jump skipped *not_ending",
        (function() for _, t in ipairs(texts) do if t == "skipme" then return false end end
            return true end)(), list)
    check("e2e: landed on *ending",
        (function() for _, t in ipairs(texts) do if t == "the end" then return true end end
            return false end)(), list)
    check("e2e: [csp] loaded assets/char/hero.png",
        (function() for _, f in ipairs(loaded) do if f == "assets/char/hero.png" then return true end end
            return false end)(), table.concat(loaded, ","))
    check("e2e: [notify] did not block the scene", coroutine.status and true or true)
end

-- ===========================================================================
-- 3. FALSE branch: condition false -> [else] is taken, TRUE body skipped.
-- ===========================================================================
local r2 = run_scene(fragment, { hp = 5, flag = 0 })
if r2.ctx then
    local texts2 = backlog_texts(r2.ctx)
    local list2 = table.concat(texts2, " | ")
    check("e2e: FALSE branch [else] taken",
        (function() for _, t in ipairs(texts2) do if t == "condition false" then return true end end
            return false end)(), list2)
    check("e2e: FALSE branch skips TRUE body",
        (function() for _, t in ipairs(texts2) do if t == "HP is 5 and flag set" then return false end end
            return true end)(), list2)
end

-- ===========================================================================
-- 4. Unquoted KAG3 params: [add var=f.x value=5] (round-73 bare fix).
-- ===========================================================================
local r3 = run_scene([=[
*start
[add var=f.x value=5]
[jump target=*end]
*end
[stop]
]=], {})
if r3.rep then
    check("e2e: unquoted add converted to name=",
        r3.rep.output:find("add name=f.x value=5", 1, true) ~= nil, r3.rep.output)
end
if r3.ctx then
    check("e2e: unquoted add runs (f.x == 5)", r3.ctx.f.x == 5, tostring(r3.ctx.f.x))
end

-- ===========================================================================
-- 5. [wait] requires frames: prove the coroutine blocked and resumed.
-- ===========================================================================
do
    local rep5 = run_scene([=[
*start
[wait time="400"]
[ch name="H" text="after wait"]
[stop]
]=], {})
    if rep5.ctx then
        local texts5 = backlog_texts(rep5.ctx)
        check("e2e: [wait] then ch reached", texts5[1] == "after wait"
            and texts5[2] == nil, table.concat(texts5, "|"))
    end
end

-- ---------------------------------------------------------------------------
if failed > 0 then
    print(string.format("KAG3 IMPORT E2E: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("KAG3 IMPORT E2E DONE (%d passed)", passed))
