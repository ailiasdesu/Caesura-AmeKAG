-- =============================================================================
--  test_flow_edge_call.lua — Scheduler [call]/[return] call-stack depth
--  boundary tests (Round 84 B-group focused on the call body itself).
--
--  Round 83 verified goto x call mixing (test_flow_edge.lua B-group). This
--  suite pins the call/return frame stack directly:
--      E. nested call depth            (3-level LIFO unwind; 20-level no crash)
--      F. call parameter / lf isolation (callee dirties lf, caller restored)
--      G. bare return / cross-scene     (no-frame return terminates;
--         cross-scene call->return scene restore; macro-body return)
--      H. call x switch / for           (call per-iteration; call into case;
--         return from inside a case)
--      I. call x saveplace/loadplace    (round-74: saveplace does not serialize
--         the call stack, so a post-load [return] becomes a bare return)
--
--  Methodology mirrors test_flow_edge.lua: lock OBSERVED scheduler behavior
--  (implementation authoritative); a frame cap prevents any hang. Defect
--  windows are bracketed "[DEFECT-window]" and scripts/scheduler.lua is left
--  untouched for the main agent to adjudicate.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local scheduler = require("scheduler")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Flow-Edge-Call (call/return depth boundary) Tests ===\n")

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, macro_args = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
    }
end

local kag_orig = package.loaded["kag"]
local kmock = {}
kmock.ch = function(ctx, p)
    ctx.dispatched[#ctx.dispatched + 1] = { cmd = "ch", params = p }
end
kmock.bg = function(ctx, p)
    ctx.dispatched[#ctx.dispatched + 1] = { cmd = "bg", params = p }
end
kmock.play_bgm = function() end
kmock.wait = function() end
kmock.quake = function() end
package.loaded["kag"] = kmock

local function collect_tags(ctx)
    local o = {}
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.tag then o[#o + 1] = d.params.tag end
    end
    return table.concat(o, ",")
end

local function run_capped(ctx, tokens, maxf)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    local n = 0
    local M = maxf or 2000
    while coroutine.status(co) ~= "dead" and n < M do
        n = n + 1
        coroutine.resume(co, 16)
    end
    return n, coroutine.status(co)
end

-- ---------------------------------------------------------------------------
-- E. call/return depth boundaries
-- ---------------------------------------------------------------------------

-- E1. 3-level nested [call]/[return]: each callee writes its own lf frame; a
--     shared probe list records the lf.x each level sees, proving the frames
--     are isolated and restored LIFO. (Task 1)
do
    local ctx = make_ctx()
    ctx.f._notes = {}
    local n, st = run_capped(ctx, {
        { "ch", { tag = "START" } },
        { "call", { target = "*a" } },
        { "jump", { target = "*DONE" } },
        { "label", { name = "a" } },
            { "ch", { tag = "A1" } },
            { "eval", { exp = "lf.x = 'a'" } },
            { "eval", { exp = "ctx.f._notes[#ctx.f._notes+1] = 'a:'..lf.x" } },
            { "call", { target = "*b" } },
            { "eval", { exp = "ctx.f._notes[#ctx.f._notes+1] = 'a.back:'..lf.x" } },
            { "return", {} },
        { "label", { name = "b" } },
            { "ch", { tag = "B1" } },
            { "eval", { exp = "lf.x = 'b'" } },
            { "eval", { exp = "ctx.f._notes[#ctx.f._notes+1] = 'b:'..lf.x" } },
            { "call", { target = "*c" } },
            { "eval", { exp = "ctx.f._notes[#ctx.f._notes+1] = 'b.back:'..lf.x" } },
            { "return", {} },
        { "label", { name = "c" } },
            { "ch", { tag = "C1" } },
            { "eval", { exp = "lf.x = 'c'" } },
            { "eval", { exp = "ctx.f._notes[#ctx.f._notes+1] = 'c:'..lf.x" } },
            { "return", {} },
        { "label", { name = "DONE" } },
        { "ch", { tag = "AFTER" } },
    }, 500)
    check("E1 3-level nested call terminates", n < 500 and st == "dead", "frames " .. n)
    check("E1 nested call unwinds LIFO clean",
        collect_tags(ctx) == "START,A1,B1,C1,AFTER", collect_tags(ctx))
    check("E1 lf frames isolated per level (a:a,b:b,c:c at own depth)",
        table.concat(ctx.f._notes, ",") == "a:a,b:b,c:c,b.back:b,a.back:a",
        tostring(table.concat(ctx.f._notes, ",")))
    check("E1 caller frames restored LIFO on unwind (b.back:b then a.back:a)",
        ctx.f._notes[4] == "b.back:b" and ctx.f._notes[5] == "a.back:a",
        tostring(ctx.f._notes[4]) .. " | " .. tostring(ctx.f._notes[5]))
    check("E1 no call-frame leak after full unwind", #(ctx.call_stack or {}) == 0,
        tostring(#(ctx.call_stack or {})))
end

-- E2. Deep nesting (20 levels) does not crash and fully unwinds. (Task 1)
do
    local ctx = make_ctx()
    local toks = {
        { "ch", { tag = "TOP" } },
        { "call", { target = "*L1" } },
        { "jump", { target = "*POST" } },
    }
    for i = 1, 19 do
        toks[#toks + 1] = { "label", { name = "L" .. i } }
        toks[#toks + 1] = { "call", { target = "*L" .. (i + 1) } }
        toks[#toks + 1] = { "return", {} }
    end
    toks[#toks + 1] = { "label", { name = "L20" } }
    toks[#toks + 1] = { "ch", { tag = "DEEP20" } }
    toks[#toks + 1] = { "return", {} }
    toks[#toks + 1] = { "label", { name = "POST" } }
    toks[#toks + 1] = { "ch", { tag = "POST" } }
    local n, st = run_capped(ctx, toks, 4000)
    check("E2 20-level nested call terminates (no crash/hang)",
        n < 4000 and st == "dead", "frames " .. n)
    check("E2 deep call reaches innermost then unwinds",
        collect_tags(ctx) == "TOP,DEEP20,POST", collect_tags(ctx))
    check("E2 deep call leaves no frame leak", #(ctx.call_stack or {}) == 0,
        tostring(#(ctx.call_stack or {})))
end

-- ---------------------------------------------------------------------------
-- F. call parameter / lf frame isolation (Task 2)
-- ---------------------------------------------------------------------------

-- F1. A callee overwrites lf.sentinel + creates lf.extra; after [return] the
--     caller's lf frame is restored (sentinel back, extra gone). f.* (the
--     shared variable table) is NOT frame-isolated -- locking the documented
--     "only lf is per-call-frame" contract.
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "eval", { exp = "lf.sentinel = 'outer'" } },
        { "call", { target = "*sub" } },
        { "ch", { tag = "RETURNED" } },
        { "jump", { target = "*DONE" } },
        { "label", { name = "sub" } },
            { "ch", { tag = "SUBENTRY" } },
            { "eval", { exp = "lf.sentinel = 'inner'" } },
            { "eval", { exp = "lf.extra = 'created'" } },
            { "eval", { exp = "f.global = 'g'" } },
            { "ch", { tag = "SUBMID" } },
            { "return", {} },
        { "label", { name = "DONE" } },
        { "ch", { tag = "DONE" } },
    }, 400)
    check("F1 call terminates", n < 400 and st == "dead", "frames " .. n)
    check("F1 callee body then caller resume (SUBENTRY,SUBMID,RETURNED,DONE)",
        collect_tags(ctx) == "SUBENTRY,SUBMID,RETURNED,DONE", collect_tags(ctx))
    check("F1 caller lf.sentinel restored to 'outer' after return",
        ctx.lf.sentinel == "outer", tostring(ctx.lf.sentinel))
    check("F1 callee-created lf.extra not leaked to caller",
        ctx.lf.extra == nil, tostring(ctx.lf.extra))
    check("F1 f.* shared across call frames (f.global persists)",
        ctx.f.global == "g", tostring(ctx.f.global))
end

-- ---------------------------------------------------------------------------
-- G. bare return / cross-scene call return (Task 3)
-- ---------------------------------------------------------------------------

-- G1. Bare [return] with NO call frame terminates execution (round 53:
--     "no crash"; scheduler treats it as end).
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "START" } },
        { "return", {} },
        { "ch", { tag = "NEVER" } },
    }, 200)
    check("G1 bare return terminates (no crash)", n < 200 and st == "dead",
        "frames " .. n .. " st=" .. tostring(st))
    check("G1 bare return stops before trailing token",
        collect_tags(ctx) == "START", collect_tags(ctx))
    check("G1 bare return leaves no frame", #(ctx.call_stack or {}) == 0,
        tostring(#(ctx.call_stack or {})))
end

-- G2. Cross-scene [call path.ks] -> [return]: tokens + scene swap to the
--     callee and back to the caller; scene name is restored.
do
    local sub_tokens = {
        { "ch", { tag = "SUB_ENTRY" } },
        { "return", {} },
    }
    local ctx = make_ctx()
    ctx.current_scene = "main.ks"
    ctx.load_tokens = function(path)
        ctx._loadedPath = path
        return sub_tokens
    end
    local n, st = run_capped(ctx, {
        { "ch", { tag = "MAIN_START" } },
        { "call", { target = "sub.ks" } },
        { "ch", { tag = "MAIN_BACK" } },
    }, 300)
    check("G2 cross-scene call->return terminates", n < 300 and st == "dead",
        "frames " .. n)
    check("G2 callee ran then caller resumed (MAIN_START,SUB_ENTRY,MAIN_BACK)",
        collect_tags(ctx) == "MAIN_START,SUB_ENTRY,MAIN_BACK", collect_tags(ctx))
    check("G2 cross-scene scene restored to caller on return",
        ctx.current_scene == "main.ks", tostring(ctx.current_scene))
    check("G2 load_tokens hit the allowlisted path",
        ctx._loadedPath == "assets/script/sub.ks", tostring(ctx._loadedPath))
    check("G2 cross-scene return leaves no frame leak",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- G3. [return] inside a macro body: the static-safe macro is inlined at
--     compile time, so the [return] runs as a bare return and terminates the
--     stream (tokens after the macro call never run). (Task 3/5)
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "BEFORE" } },
        { "macro", { name = "m_ret" } },
            { "ch", { tag = "MACRO_RET" } },
            { "return", {} },
        { "endmacro", {} },
        { "m_ret", {} },
        { "ch", { tag = "AFTER" } },
    }, 300)
    check("G3 macro-body bare return terminates (no hang)",
        n < 300 and st == "dead", "frames " .. n)
    check("G3 macro body ran, post-macro token stopped",
        collect_tags(ctx) == "BEFORE,MACRO_RET", collect_tags(ctx))
    check("G3 macro-body return left no call frame",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- ---------------------------------------------------------------------------
-- H. call x switch / for (Task 6)
-- ---------------------------------------------------------------------------

-- H1. [call] inside a [for] body: the subroutine runs per iteration and
--     returns to the caller loop body each time.
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "for", { var = "i", start = "1", ["end"] = "3" } },
            { "ch", { tag = "FBODY" } },
            { "call", { target = "*fsub" } },
            { "ch", { tag = "FAFTER" } },
        { "endfor", {} },
        { "jump", { target = "*FEND" } },
        { "label", { name = "fsub" } },
            { "ch", { tag = "FSUBCALL" } },
            { "return", {} },
        { "label", { name = "FEND" } },
        { "ch", { tag = "FDONE" } },
    }, 500)
    check("H1 call-in-for terminates", n < 500 and st == "dead", "frames " .. n)
    check("H1 call runs per iteration (3x FBODY/FSUBCALL/FAFTER)",
        collect_tags(ctx)
            == "FBODY,FSUBCALL,FAFTER,FBODY,FSUBCALL,FAFTER,FBODY,FSUBCALL,FAFTER,FDONE",
        collect_tags(ctx))
    check("H1 call-in-for leaves no frame leak",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
    check("H1 for counter settles past end", ctx.f.i == 4, tostring(ctx.f.i))
end

-- H2. [call] INTO a switch case body (nested [call]) and back. (Task 6)
do
    local ctx = make_ctx(); ctx.f.mode = "x"
    local n, st = run_capped(ctx, {
        { "call", { target = "*sw" } },
        { "ch", { tag = "SW_RETURNED" } },
        { "jump", { target = "*SWEND" } },
        { "label", { name = "sw" } },
            { "switch", { exp = "f.mode" } },
            { "case", { "x" } }, { "ch", { tag = "CASE_X" } },
                { "call", { target = "*inner" } },
                { "ch", { tag = "CASE_X_BACK" } },
            { "case", { "y" } }, { "ch", { tag = "CASE_Y" } },
            { "endswitch", {} },
            { "ch", { tag = "SW_FALL" } },
            { "return", {} },
        { "label", { name = "inner" } },
            { "ch", { tag = "INNER_CALL" } },
            { "return", {} },
        { "label", { name = "SWEND" } },
    }, 400)
    check("H2 call into switch case terminates", n < 400 and st == "dead",
        "frames " .. n)
    check("H2 case body + nested call + return to caller",
        collect_tags(ctx) == "CASE_X,INNER_CALL,CASE_X_BACK,SW_FALL,SW_RETURNED",
        collect_tags(ctx))
    check("H2 no call-frame leak after switch-in-call",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- H3. [return] OUT of a called subroutine from INSIDE a switch case.
--     Observed: the return pops the call frame and resumes the caller after
--     the [call]; CASE_B / SW_FALL never run. The caller's trailing [jump]
--     resets the loop/switch stacks, so the switch entry does NOT leak
--     (round-83 A4/A5 jump stack reset also covers the returning-out-of-switch
--     path) -- switch_stack stays empty. No defect.
do
    local ctx = make_ctx(); ctx.f.mode = "a"
    local n, st = run_capped(ctx, {
        { "call", { target = "*sw" } },
        { "ch", { tag = "SW_RETURNED" } },
        { "jump", { target = "*SWEND" } },
        { "label", { name = "sw" } },
            { "switch", { exp = "f.mode" } },
            { "case", { "a" } }, { "ch", { tag = "CASE_A" } },
                { "return", {} },   -- return out of the switch inside the callee
            { "case", { "b" } }, { "ch", { tag = "CASE_B" } },
            { "endswitch", {} },
            { "ch", { tag = "SW_FALL" } },
            { "return", {} },
        { "label", { name = "SWEND" } },
    }, 300)
    check("H3 return-from-inside-case resumes caller (no crash)",
        collect_tags(ctx) == "CASE_A,SW_RETURNED", collect_tags(ctx))
    check("H3 case-b / fall-through after return are skipped",
        collect_tags(ctx):match("CASE_B") == nil
            and collect_tags(ctx):match("SW_FALL") == nil, collect_tags(ctx))
    check("H3 return-out-of-call leaves no call-frame leak",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
    check("H3 switch stack stays consistent (jump reset, no leak)",
        #(ctx._switchStack or {}) == 0, "switchStack=" .. tostring(#(ctx._switchStack or {})))
end

-- ---------------------------------------------------------------------------
-- I. call x saveplace/loadplace (Task 4 — round 74 boundary)
-- ---------------------------------------------------------------------------

-- I1. [saveplace] taken inside a live call frame: the bookmark captures the
--     INNER scene + token position but deliberately NOT the call_stack --
--     the documented round-74 boundary (System-level lock).
do
    local System = require("system")
    local had = System._placeData
    local ctxFrame = { f = { inner = 1 }, tf = {}, sf = {}, mp = {}, lf = {},
        variables = {}, current_scene = "assets/script/demo_sub.ks",
        token_index = 9, call_stack = {
            { tokens = { "main" }, index = 3, scene = "assets/script/main.ks", lf = { outer = 1 } },
            { tokens = { "mid" }, index = 7, scene = "assets/script/mid.ks", lf = {} },
        } }
    System.saveplace(ctxFrame)
    local pd = System._placeData
    check("I1 saveplace-in-call captures inner scene",
        pd and pd.scene == "assets/script/demo_sub.ks", tostring(pd and pd.scene))
    check("I1 saveplace-in-call captures inner token index",
        pd and pd.index == 9, tostring(pd and pd.index))
    check("I1 saveplace does NOT serialize the call stack (round 74)",
        pd and pd.call_stack == nil, "call_stack present in bookmark")
    check("I1 saveplace carries tf payload intact", pd and pd.tf ~= nil, "")
    if had == nil then System._placeData = nil end
end

-- I2. After a loadplace resume (which clears the call stack), a [return] in
--     the resumed stream is a BARE return (no frame) -> terminates the run —
--     no crash, no redirect into a stale frame. Locks round-74 "return 变裸
--     return". (Scheduler-level.)
do
    local ctx = make_ctx()
    -- simulate post-loadplace/post-load state: run resumes with EMPTY stack
    ctx.call_stack = {}
    local n, st = run_capped(ctx, {
        { "ch", { tag = "R1" } },
        { "return", {} },
        { "ch", { tag = "R2" } },
    }, 200)
    check("I2 bare return after load-style empty stack terminates",
        n < 200 and st == "dead", "frames " .. n)
    check("I2 post-load bare return does not resume stale tokens",
        collect_tags(ctx) == "R1", collect_tags(ctx))
    check("I2 post-load bare return leaks no frame",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

package.loaded["kag"] = kag_orig

print(string.format("\nFlow-edge-call results: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
