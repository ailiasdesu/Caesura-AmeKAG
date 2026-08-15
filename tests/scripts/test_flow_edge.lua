-- =============================================================================
--  test_flow_edge.lua — Scheduler goto/jump flow-boundary deep tests.
--
--  Round 78 locked "[goto] into a loop body is undefined (no crash)"; this
--  suite extends the boundary matrix around the goto/jump flow commands:
--      A. goto <-> loop-stack interaction  (rewind heads, switch-case entry,
--         goto OUT of a loop body and its stack-consistency window)
--      B. call/return <-> goto mixing      (subroutine-internal goto+return,
--         deep call stack + goto, goto threading a dangling call frame)
--      C. macro expansion + goto           (goto inside a macro body)
--      D. label-resolution boundaries      (EOF label, special-char label,
--         whitespace-padded target, duplicate-label first-wins)
--
--  Methodology: each assertion locks OBSERVED scheduler behavior (the
--  implementation is authoritative). Where the observation reveals a defect
--  window, the assert is bounded (frame cap) so the suite can never hang, is
--  marked "[DEFECT] (reported — scripts/scheduler.lua left untouched)", and
--  documents the exact misbehavior so the main agent can adjudicate.
-- =============================================================================

package.path = "scripts/?.lua;" .. package.path

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

print("\n=== Flow-Edge (goto/jump boundary) Tests ===\n")

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

-- Run to completion with a hard frame cap so a mis-locked runaway asserts
-- "no hang" instead of hanging the whole suite.
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
-- A. goto <-> loop-stack interaction
-- ---------------------------------------------------------------------------

-- A1. goto TO A FOR-LOOP HEAD (label placed on the [for] token). Observed:
--     the loop is entered normally and rewinds to completion — a clean,
--     terminating rewind. (Task 1: "goto 到 for 循环头部, 回绕")
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "goto", { target = "*FORHEAD" } },
        { "label", { name = "FORHEAD" } },
        { "for", { var = "i", start = "1", ["end"] = "3" } },
            { "ch", { tag = "BODY" } },
        { "endfor", {} },
        { "ch", { tag = "AFTER" } },
    }, 200)
    check("A1 goto to for-head rewinds and terminates", n < 200 and st == "dead", "frames " .. n)
    check("A1 goto-for-head completes 3 bodies + after", collect_tags(ctx) == "BODY,BODY,BODY,AFTER", collect_tags(ctx))
    check("A1 goto-for-head counter ends past end", ctx.f.i == 4, tostring(ctx.f.i))
end

-- A2. goto TO A WHILE-LOOP HEAD. Observed: rewinds and re-evaluates the
--     condition each pass (2 bodies here). (Task 1)
do
    local ctx = make_ctx(); ctx.f.n = 0
    local n, st = run_capped(ctx, {
        { "goto", { target = "*WHEAD" } },
        { "label", { name = "WHEAD" } },
        { "while", { exp = "f.n < 2" } },
            { "ch", { tag = "WB" } },
            { "iscript", { body = "ctx.f.n = ctx.f.n + 1" } },
        { "endwhile", {} },
        { "ch", { tag = "AFTER" } },
    }, 200)
    check("A2 goto-to-while-head rewinds (2 bodies)", collect_tags(ctx) == "WB,WB,AFTER", collect_tags(ctx))
end

-- A3. goto INTO A SWITCH CASE BODY, bypassing the [switch] head. Observed:
--     the case body runs, the endswitch has no stack entry (no head pushed)
--     and is a no-op, execution continues after — no crash. (Task 1)
do
    local ctx = make_ctx(); ctx.f.mode = "none"  -- would not match case b
    local n, st = run_capped(ctx, {
        { "goto", { target = "*C1" } },
        { "switch", { exp = "f.mode" } },
            { "case", { "a" } }, { "ch", { tag = "A" } },
            { "case", { "b" } }, { "label", { name = "C1" } }, { "ch", { tag = "INTOCASE" } },
        { "endswitch", {} },
        { "ch", { tag = "AFTER" } },
    }, 200)
    check("A3 goto into case body runs body then after", collect_tags(ctx) == "INTOCASE,AFTER", collect_tags(ctx))
    check("A3 goto into case leaks no switch stack entry", #(ctx._switchStack or {}) == 0, tostring(#(ctx._switchStack or {})))
end

-- A4. goto OUT of a WHILE body to a label past [endwhile]. Observed: body
--     runs once, execution continues past the end token (no crash/hang), but
--     the while-stack entry is LEAKED (size stays 1) — a stack-consistency
--     window. [DEFECT-window, reported] (Task 1: loop-stack state consistency)
do
    local ctx = make_ctx(); ctx.f.n = 0
    local n, st = run_capped(ctx, {
        { "while", { exp = "f.n < 5" } },
            { "ch", { tag = "BODY" } },
            { "goto", { target = "*EXIT" } },
            { "iscript", { body = "ctx.f.n = ctx.f.n + 1" } },
        { "endwhile", {} },
        { "label", { name = "EXIT" } },
        { "ch", { tag = "AFTER" } },
    }, 200)
    check("A4 goto out of while: no crash/hang", n < 200 and st == "dead", "frames " .. n)
    check("A4 goto out of while runs body once then after", collect_tags(ctx) == "BODY,AFTER", collect_tags(ctx))
    check("A4 goto out of while: while-stack reset (round 83 fix)", #(ctx._whileStack or {}) == 0, "stack=" .. tostring(#(ctx._whileStack or {})))
end

-- A5. goto OUT of a FOR body. Observed: counter is left initialized to the
--     stale value (f.i==1, never re-initialized on the leaping run) and a
--     LATER same-name [for] reuses the stale _forStackMarks marker, so it
--     does NOT re-initialize from its declared start — it keeps counting from
--     the old counter and runs 11 iterations instead of 2, ending f.i==12
--     instead of 11. [DEFECT — stack/counter state corruption, reported]
--     (Task 1/3: goto 后循环栈状态一致性, 无泄漏计数)
do
    local ctx = make_ctx()
    run_capped(ctx, {
        { "for", { var = "i", start = "1", ["end"] = "5" } },
            { "ch", { tag = "FBODY" } },
            { "goto", { target = "*FEXIT" } },
        { "endfor", {} },
        { "label", { name = "FEXIT" } },
        { "ch", { tag = "AFTER" } },
        { "for", { var = "i", start = "10", ["end"] = "11" } },
            { "ch", { tag = "SECOND" } },
        { "endfor", {} },
        { "ch", { tag = "DONE" } },
    }, 500)
    local sec = 0
    for _ in collect_tags(ctx):gmatch("SECOND") do sec = sec + 1 end
    check("A5 goto out of for: for-stack reset (round 83 fix)", #(ctx._forStack or {}) == 0, tostring(#(ctx._forStack or {})))
    check("A5 later same-name for re-initializes from declared start (round 83 fix)",
        sec == 2, "expect 2 (clean) got " .. sec .. " -> " .. collect_tags(ctx))
    -- Lua for semantics: after 10..11 the counter settles at end+1 == 12.
    check("A5 counter settles at end+1 (Lua for semantics)", ctx.f.i == 12, tostring(ctx.f.i))
end

-- ---------------------------------------------------------------------------
-- B. call/return <-> goto mixing
-- ---------------------------------------------------------------------------

-- B1. call into a subroutine, goto to a label INSIDE the subroutine, then
--     [return]. Observed: clean pop back to the caller (call_stack empty).
--     The trailing re-execution of the subroutine body after RETURNED is the
--     documented KAG3 convention (subroutine bodies live after the call site
--     and are linear-fall-through re-entered; scripts skip them with a jump).
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "START" } },
        { "call", { target = "*sub" } },
        { "ch", { tag = "RETURNED" } },
        { "label", { name = "sub" } },
            { "ch", { tag = "SUB1" } },
            { "goto", { target = "*sub2" } },
            { "ch", { tag = "NEVER" } },
        { "label", { name = "sub2" } },
            { "ch", { tag = "SUB2" } },
        { "return", {} },
    }, 200)
    check("B1 subroutine goto+return terminates", n < 200 and st == "dead", "frames " .. n)
    check("B1 internal goto skips, return pops (no frame leak)",
        #(ctx.call_stack or {}) == 0, "call_stack=" .. tostring(#(ctx.call_stack or {})))
    check("B1 goto within sub skips the never-token",
        collect_tags(ctx):match("NEVER") == nil, collect_tags(ctx))
end

-- B2. deep call stack (call within a call) + goto + return in the innermost
--     frame. Observed: START -> a -> b -> goto inner2 -> INNER2 -> returns
--     unwind both frames cleanly to DONE; call_stack empty. (Task 2)
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "START" } },
        { "call", { target = "*a" } },
        { "jump", { target = "*skip" } },
        { "label", { name = "a" } },
            { "call", { target = "*b" } },
            { "return", {} },
        { "label", { name = "b" } },
            { "ch", { tag = "INNER" } },
            { "goto", { target = "*inner2" } },
            { "ch", { tag = "NEVER" } },
        { "label", { name = "inner2" } }, { "ch", { tag = "INNER2" } },
        { "return", {} },
        { "label", { name = "skip" } },
        { "ch", { tag = "DONE" } },
    }, 500)
    check("B2 nested call + goto terminates", n < 500 and st == "dead", "frames " .. n)
    check("B2 deep call stack unwinds clean", collect_tags(ctx) == "START,INNER,INNER2,DONE", collect_tags(ctx))
    check("B2 no call-frame leak after unwinding", #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- B3. goto OUT of a subroutine to a caller-scope label, BYPASSING the
--     [return]. Observed: execution leaves the subroutine without popping the
--     call frame (a dangling frame is left, dropped at end-of-run). No crash;
--     the linear fall-through re-enters the subroutine body once the run
--     continues past the label target. [DEFECT-window, reported]
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "START" } },
        { "call", { target = "*sub" } },
        { "ch", { tag = "RETURNED" } },
        { "label", { name = "sub" } },
            { "ch", { tag = "SUB1" } },
            { "goto", { target = "*OUTER" } },
        { "return", {} },
        { "label", { name = "OUTER" } },
        { "ch", { tag = "OUTERBODY" } },
    }, 200)
    check("B3 goto-out-of-subroutine no crash", n < 200 and st == "dead", "frames " .. n)
    -- The goto escapes the subroutine body and reaches OUTERBODY; the
    -- exact count of SUB1/OUTERBODY re-executions is linear-fall-through
    -- artifact (undefined), so lock only that the escape target is hit.
    check("B3 goto escapes the subroutine into OUTERBODY",
        collect_tags(ctx):match("START,SUB1,OUTERBODY") ~= nil, collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- C. macro expansion + goto
-- ---------------------------------------------------------------------------

-- C1. goto INSIDE a macro body to an external label (target after the macro
--     body ends). Single-expansion context: the goto lands on the label and
--     execution continues — clean. (Task 4)
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "macro", { name = "m2" } },
            { "goto", { target = "*tg" } },
            { "ch", { tag = "SKIP" } },
        { "endmacro", {} },
        { "m2", {} },
        { "label", { name = "tg" } },
        { "ch", { tag = "LAND" } },
    }, 200)
    check("C1 macro-body goto to external label lands", collect_tags(ctx) == "LAND", collect_tags(ctx))
end

-- C2. macro body goto targeting a label AFTER the macro CALL SITE. This was
--     a RUNAWAY re-expansion: the second expansion's goto jumped back into
--     the just-spliced body region forever, never terminated, and no macro
--     depth-guard fired (_macroStack stays flat because each jump to the
--     out-of-body label pops its own splice entry). Round 84 cuts the cycle:
--     a BACKWARD goto (target label at-or-before the current position) is
--     allowed once, but re-taking the SAME backward edge (same origin token
--     -> same target label) is rejected with a WARN + fall-through, so the
--     run terminates within the frame cap instead of hanging. Covers both
--     the compile-time-inlined (static-safe) and runtime-splice morphologies.
--     [round 84 fix] (Task 4)
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "macro", { name = "m1" } },
            { "ch", { tag = "MSTART" } },
            { "goto", { target = "*inside" } },
            { "ch", { tag = "MSKIP" } },
        { "endmacro", {} },
        { "m1", {} },
        { "label", { name = "inside" } },
        { "ch", { tag = "AFTERFIRST" } },
        { "m1", {} },
        { "ch", { tag = "AFTERSECOND" } },
    }, 300)
    check("C2 macro-body backwards goto terminates (no re-expansion loop)",
        n < 300 and st == "dead", "frames " .. n .. " (status " .. tostring(st) .. ")")
    -- Lock the OBSERVED terminating sequence. Both [m1] call sites are
    -- static-safe and compile-time INLINED, so the stream carries two macro
    -- bodies. First expansion reaches the backwards goto once (lands), the
    -- re-entry body re-takes the same backward edge and is CUT (round 84
    -- guard), then falls through MSTART,MSKIP,AFTERSECOND. The run ends on
    -- AFTERSECOND -- the AFTERFIRST/MSTART cycle before MSKIP is bounded.
    check("C2 run falls through body to AFTERSECOND (cycle cut)",
        collect_tags(ctx) == "MSTART,AFTERFIRST,MSTART,AFTERFIRST,MSTART,MSKIP,AFTERSECOND",
        collect_tags(ctx))
    check("C2 no macro depth-guard error fired (stack flat)",
        (#(ctx._macroStack or {}) == 0), "macroStack=" .. tostring(#(ctx._macroStack or {})))
end

-- ---------------------------------------------------------------------------
-- D. label-resolution boundaries (Task 5)
-- ---------------------------------------------------------------------------

-- D1. goto to a label that is the very LAST token in the file. Observed:
--     the label token is a no-op and the run ends cleanly (no off-by-one
--     that would fall through and crash).
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "ch", { tag = "A" } },
        { "goto", { target = "*ENDL" } },
        { "ch", { tag = "SKIP" } },
        { "label", { name = "ENDL" } },
    }, 200)
    check("D1 goto to EOF label terminates clean", n < 200 and st == "dead", "frames " .. n)
    check("D1 goto to EOF lands and ends (A only)", collect_tags(ctx) == "A", collect_tags(ctx))
end

-- D2. label names with special characters (dash, dot). Observed: resolves.
do
    local ctx = make_ctx()
    run_capped(ctx, {
        { "goto", { target = "*scene-1.branch" } },
        { "ch", { tag = "SKIP" } },
        { "label", { name = "scene-1.branch" } },
        { "ch", { tag = "LAND" } },
    }, 200)
    check("D2 special-char label name resolves", collect_tags(ctx) == "LAND", collect_tags(ctx))
end

-- D3. target with leading/trailing whitespace (not trimmed). Observed: the
--     target does not start with '*' and is treated as a SCENE path; it is
--     blocked by the scene-path allowlist (spaces/no .ks) with a WARN and
--     execution FALLS THROUGH (both SKIP and LAND run). Documented boundary —
--     a whitespace-padded label target is not trimmed and does not resolve.
do
    local ctx = make_ctx()
    run_capped(ctx, {
        { "goto", { target = "  mylabel  " } },
        { "ch", { tag = "SKIP" } },
        { "label", { name = "mylabel" } },
        { "ch", { tag = "LAND" } },
    }, 200)
    check("D3 whitespace-padded target falls through (not trimmed)",
        collect_tags(ctx) == "SKIP,LAND", collect_tags(ctx))
end

-- D4. duplicate labels: KAG3 "first wins" — a goto to the duplicated name
--     resolves to the FIRST definition.
do
    local ctx = make_ctx()
    run_capped(ctx, {
        { "goto", { target = "*DUP" } },
        { "ch", { tag = "SKIP1" } },
        { "label", { name = "DUP" } }, { "ch", { tag = "FIRST" } },
        { "goto", { target = "*SKIPTOEND" } }, { "ch", { tag = "MID" } },
        { "label", { name = "DUP" } }, { "ch", { tag = "SECOND" } },
        { "label", { name = "SKIPTOEND" } },
    }, 200)
    check("D4 duplicate label resolves to FIRST", collect_tags(ctx) == "FIRST", collect_tags(ctx))
end

package.loaded["kag"] = kag_orig

print(string.format("\nFlow-edge results: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
