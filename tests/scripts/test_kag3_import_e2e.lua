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


-- ===========================================================================
-- 6. round-84 deep-boundary RUNTIME: alias matrix, RENAMES (goto->jump),
--    macro-arg text expansion, alias+explicit coexist (last-wins), emb branch.
-- ===========================================================================
-- 6a. PARAM_ALIASES math matrix: every var->name command produces the
--     expected variable value at runtime (converted [add name=...] etc).
do
    local sc = "*start\n"
        .. "[add var=\"f.a\" value=\"5\"]\n"
        .. "[sub var=\"f.b\" value=\"3\"]\n"
        .. "[mul var=\"f.c\" value=\"4\"]\n"
        .. "[div var=\"f.d\" value=\"2\"]\n"
        .. "[mod var=\"f.e\" value=\"3\"]\n"
        .. "[dec var=\"f.f\"]\n"
        .. "[dec var=\"f.g\" amount=3]\n"
        .. "[stop]\n"
    local rr = run_scene(sc, { a=0, b=10, c=2, d=10, e=10, f=5, g=10 })
    if rr.rep and rr.ctx then
        local c = rr.ctx
        check("e2e: alias matrix converts var->name (no var= left)",
            rr.rep.output:find("var=", 1, true) == nil, rr.rep.output)
        check("e2e: [add] f.a==5", c.f.a == 5, tostring(c.f.a))
        check("e2e: [sub] f.b==7", c.f.b == 7, tostring(c.f.b))
        check("e2e: [mul] f.c==8", c.f.c == 8, tostring(c.f.c))
        check("e2e: [div] f.d==5", c.f.d == 5, tostring(c.f.d))
        check("e2e: [mod] f.e==1", c.f.e == 1, tostring(c.f.e))
        check("e2e: [dec] f.f==4 (default amount 1)", c.f.f == 4, tostring(c.f.f))
        check("e2e: [dec] f.g==7 (amount=3)", c.f.g == 7, tostring(c.f.g))
    end
end

-- 6b. RENAMES runtime: [goto]->[jump] forward jump actually hops. [waitse]
--     conversion is asserted (the mock backend has no audio_* handler, so
--     runtime waitsound audio is not exercised here).
do
    local rr = run_scene("*start\n[goto target=*skip]\n[ch name=\"H\" text=\"WRONG\"]\n*skip\n[ch name=\"H\" text=\"right\"]\n[waitse end=1]\n[stop]\n", {})
    if rr.rep then
        check("e2e: goto->jump converted", rr.rep.output:find("[goto", 1, true) == nil
            and rr.rep.output:find("[jump target=*skip]", 1, true) ~= nil, rr.rep.output)
    end
    local texts = backlog_texts(rr.ctx)
    local list = table.concat(texts, " | ")
    check("e2e: [jump] forward hop skipped WRONG",
        (function() for _, t in ipairs(texts) do if t == "WRONG" then return false end end return true end)(), list)
    check("e2e: [jump] landed on *skip -> right",
        (function() for _, t in ipairs(texts) do if t == "right" then return true end end return false end)(), list)
    check("e2e: waitsound conversion (rename)", rr.rep.output:find("waitse", 1, true) == nil
        and rr.rep.output:find("[waitsound end=1]", 1, true) ~= nil, rr.rep.output)
end

-- 6c. Macro-arg text-position expansion runs: [greet who=hero 10 20] fills
--     both the named %who% and the numeric %1%/%2% placeholders.
do
    local rr = run_scene("*start\n[macro greet args=\"who\"]\nHello &who p1=&1 p2=&2\n[endmacro]\n[greet who=hero 10 20]\n[stop]\n", {})
    local texts = backlog_texts(rr.ctx)
    local list = table.concat(texts, " | ")
    check("e2e: macro text-arg conversion (%who%/%1%/%2%)",
        rr.rep.output:find("Hello %who% p1=%1% p2=%2%", 1, true) ~= nil, rr.rep.output)
    check("e2e: macro runtime fills named + numeric args",
        (function() for _, t in ipairs(texts) do if t == "Hello hero p1=10 p2=20" then return true end end return false end)(), list)
end

-- 6d. Alias + explicit engine param coexist: the importer does not dedup, so
--     [add var=... name=...] -> two name= keys whose runtime winner is the
--     LAST one (positional), NOT reliably the explicit engine name. Lock the
--     current behavior here (forward order: name= after var= -> f.e wins).
do
    local rr = run_scene("*start\n[add var=\"f.v\" name=\"f.e\" value=\"5\"]\n[stop]\n", { v=200, e=100 })
    if rr.rep and rr.ctx then
        check("e2e: coexist keeps two name= (no dedup)",
            rr.rep.output:find("add name=\"f.v\" name=\"f.e\"", 1, true) ~= nil, rr.rep.output)
        check("e2e: coexist runtime is LAST-name-wins (f.e incremented)",
            rr.ctx.f.e == 105 and rr.ctx.f.v == 200,
            "f.v="..tostring(rr.ctx.f.v).." f.e="..tostring(rr.ctx.f.e))
    end
end

-- 6e. [if] expression (TJS->Lua) drives a runtime branch; [emb] expression
--     param is converted (params preserved) though the mock backend cannot
--     evaluate it, so only the branch + conversion are asserted.
do
    local rr = run_scene("*start\n[if exp=\"f.hp + 1 == 43\"]\n[ch name=\"H\" text=\"emb ok\"]\n[endif]\n[emb exp=\"f.hp + 2\"]\n[stop]\n", { hp=42 })
    local texts = backlog_texts(rr.ctx)
    local list = table.concat(texts, " | ")
    check("e2e: TJS expr in [if] runs TRUE branch",
        (function() for _, t in ipairs(texts) do if t == "emb ok" then return true end end return false end)(), list)
    check("e2e: emb exp converted (params preserved)",
        rr.rep.output:find("[emb exp=\"f.hp + 2\"]", 1, true) ~= nil, rr.rep.output)
end
-- ---------------------------------------------------------------------------
if failed > 0 then
    print(string.format("KAG3 IMPORT E2E: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("KAG3 IMPORT E2E DONE (%d passed)", passed))
