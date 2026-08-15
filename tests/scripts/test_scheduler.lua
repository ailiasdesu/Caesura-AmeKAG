-- =============================================================================
--  test_scheduler.lua — Scheduler flow-control unit tests
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local scheduler = require("scheduler")

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
        print(string.format("  [PASS] %s", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("\n=== Scheduler Tests ===\n")

local function run_in_coro(ctx, tokens)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    while coroutine.status(co) ~= "dead" do
        coroutine.resume(co)
    end
end

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
    }
end

-- Mock kag
local kag_mock = {}
local function mock_cmd(cmd)
    return function(ctx, params)
        ctx.dispatched[#ctx.dispatched + 1] = {cmd = cmd, params = params}
    end
end
for _, cmd in ipairs({"bg","fg","ch","play_bgm","play_voice","play_se","wait","stop","quake","flash","fade","blur","transition","pimage","ruby"}) do
    kag_mock[cmd] = mock_cmd(cmd)
end
package.loaded["kag"] = kag_mock

-- 1. Empty
do
    local ctx = make_ctx()
    run_in_coro(ctx, {})
    check("empty tokens", true)
end

-- 2. Basic dispatch
do
    local ctx = make_ctx()
    run_in_coro(ctx, {{"bg", {file = "test.jpg"}}})
    check("bg dispatched", #ctx.dispatched >= 1)
end

-- 3. [end] stops
do
    local ctx = make_ctx()
    run_in_coro(ctx, {{"end", {}}, {"bg", {file = "never.jpg"}}})
    check("end stops", #ctx.dispatched == 0)
end

-- 4. [jump] intra-scene
do
    local ctx = make_ctx()
    run_in_coro(ctx, {
        {"jump", {storage = "L1"}}, {"bg", {file = "skip.jpg"}},
        {"label", {name = "L1"}}, {"bg", {file = "jumped.jpg"}},
    })
    local found = false
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.file == "jumped.jpg" then found = true end
    end
    check("jump works", found)
end

-- 5. Scheduler.resume forwards to run
check("resume exists", type(scheduler.resume) == "function")

-- 6. Bare-text tokens (KAG3 style) become [ch text=...]
do
    local tokenizer = require("tokenizer")
    -- This file pre-loads a kag_mock (package.loaded["kag"]) whose commands
    -- record into ctx.dispatched; bare text must dispatch as [ch] with the
    -- content as params.text (KAG3 style bare dialogue lines).
    local tokenizer = require("tokenizer")
    local parsed = tokenizer.parse("Hello world!")
    local ctx = make_ctx()
    local ok = pcall(run_in_coro, ctx, parsed)
    local found = false
    for _, d in ipairs(ctx.dispatched) do
        if d.cmd == "ch" and d.params and d.params.text == "Hello world!" then
            found = true
        end
    end
    check("bare text dispatched as ch", ok and found)
end

-- 7. Malformed flow params do not kill the coroutine chain
do
    local ctx = make_ctx()
    local ok = pcall(run_in_coro, ctx, { {"jump", { target = 123 }} })
    check("malformed jump target no-crash", ok)
end

-- ---------------------------------------------------------------------------
-- 8. [until exp=... timeout=ms] — declarative conditional wait
--    (driven with explicit dt so frame progression is deterministic)
-- ---------------------------------------------------------------------------
local function run_until(ctx, tokens, maxResumes, onFrame)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    local n = 0
    while coroutine.status(co) ~= "dead" and n < maxResumes do
        n = n + 1
        if onFrame then onFrame(n) end
        coroutine.resume(co, 16)  -- 16ms per frame
    end
    return n
end

-- 8a. true immediately: no wait
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "1 == 1", timeout = 5000 }}, {"ch", { text = "done" }} }, 10)
    check("until true-immediate does not block", n == 3)
    check("until true-immediate dispatches after", #ctx.dispatched == 1)
end

-- 8b. waits until the flag flips, then proceeds
do
    local ctx = make_ctx()
    local flagSetAt = nil
    local n = run_until(ctx,
        { {"until", { exp = "f.go == 1", timeout = 5000 }}, {"ch", { text = "done" }} },
        20, function(frame)
            if frame == 3 then ctx.f.go = 1; flagSetAt = frame end
        end)
    check("until waits until condition", flagSetAt == 3 and n == 5)
    check("until dispatches after condition", #ctx.dispatched == 1
          and ctx.dispatched[1].cmd == "ch")
end

-- 8c. timeout path: never true, bounded by timeout (48ms = 3 frames)
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 48 }}, {"ch", { text = "done" }} },
        50)
    check("until times out and continues", #ctx.dispatched == 1)
    check("until timeout bounded frames", n <= 10)
end

-- 8d. empty exp is a no-op passthrough
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { timeout = 100 }}, {"ch", { text = "done" }} }, 5)
    check("until empty exp passes through", n == 3 and #ctx.dispatched == 1)
end

-- 8e. TJS syntax works through compile-time translation (&& !=)
do
    local ctx = make_ctx()
    ctx.f.a, ctx.f.b = 1, 2
    local n = run_until(ctx,
        { {"until", { exp = "f.a == 1 && f.b != 3", timeout = 5000 }},
          {"ch", { text = "done" }} }, 10)
    check("until TJS && != true-immediate", n == 3 and #ctx.dispatched == 1)
end

-- 8f. scene abort (stop_flag) ends the wait immediately
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 60000 }},
          {"ch", { text = "done" }} }, 10, function(frame)
            if frame == 2 then ctx.stop_flag = true end
        end)
    check("until aborts on stop_flag", n == 3 and #ctx.dispatched == 0)
end

-- 8g. Lua-initiated jump (_next_index) ends the wait and is honored
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 60000 }},
          {"label", { name = "later" }},
          {"ch", { text = "done" }} }, 10, function(frame)
            if frame == 2 then ctx._next_index = 2 end  -- jump to the label
        end)
    check("until honors _next_index", n == 5 and #ctx.dispatched == 1)
end

-- 8h. broken expression stops polling (no per-frame error flood)
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.x &&", timeout = 60000 }},
          {"ch", { text = "done" }} }, 10)
    check("until broken exp aborts wait", n == 3 and #ctx.dispatched == 1)
end

-- 8i-8o. [until] edge cases (round 59): timeout boundaries, default,
-- operation cancel, waiting_input non-interference, truthiness, guard.
do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 0 }}, {"ch", { text = "done" }} }, 10)
    check("until timeout=0 falls through immediately", n <= 4
          and #ctx.dispatched == 1)
    check("until timeout=0 no active ops leak", #(ctx.active_operations or {}) == 0)
end

do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.x == 1", timeout = -5 }}, {"ch", { text = "done" }} }, 10)
    check("until negative timeout clamped to 0", n <= 4 and #ctx.dispatched == 1)
end

do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1" }}, {"ch", { text = "done" }} },
        6)
    check("until default timeout does not expire early", n == 6
          and #ctx.dispatched == 0)
end

do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1" }}, {"ch", { text = "done" }} },
        6, function(frame)
            if frame == 2 then ctx.stop_flag = true end
        end)
    check("until stop_flag aborts scene", n <= 5 and ctx.stop_flag == true
          and #(ctx.active_operations or {}) == 0)
end

do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 60000 }}, {"ch", { text = "done" }} },
        6, function(frame)
            if frame == 2 then
                local t = ctx.active_operations and ctx.active_operations[1]
                if t then t:mark_cancelled() end
            end
        end)
    check("until operation cancel ends wait", n <= 4
          and #ctx.dispatched == 1
          and #(ctx.active_operations or {}) == 0)
end

do
    local ctx = make_ctx()
    ctx.waiting_input = true  -- pre-existing input wait (e.g. a [p])
    local n = run_until(ctx,
        { {"until", { exp = "f.go == 1", timeout = 5000 }}, {"ch", { text = "done" }} },
        20, function(frame)
            if frame == 3 then ctx.f.go = 1 end
        end)
    check("until does not touch waiting_input", n <= 8
          and ctx.waiting_input == true and #ctx.dispatched == 1)
end

do
    local ctx = make_ctx()
    ctx.f.cnt = 0
    local n = run_until(ctx,
        { {"until", { exp = "f.cnt", timeout = 5000 }}, {"ch", { text = "done" }} },
        20, function(frame)
            if frame == 3 then ctx.f.cnt = 1 end
        end)
    check("until non-boolean truthiness waits", n <= 8
          and ctx.f.cnt == 1 and #ctx.dispatched == 1)
end

do
    local ctx = make_ctx()
    local n = run_until(ctx,
        { {"until", { exp = "f.never == 1", timeout = 16 }}, {"ch", { text = "done" }} },
        10)
    check("until 1-frame timeout continues", n <= 5 and #ctx.dispatched == 1
          and #(ctx.active_operations or {}) == 0)
end


-- 9. [switch]/[case]/[default] — real assertions (replaces former
--    hard-coded fake passes). Hand-built token streams exercise the
--    runtime depth-aware scan path.
do
    -- dispatch helper that tags which case body ran
    local function run_switch(f, tokens)
        local ctx = make_ctx()
        ctx.f = f or {}
        local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
        while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
        local out = {}
        for _, d in ipairs(ctx.dispatched) do
            out[#out + 1] = (d.params and d.params.tag) or d.cmd
        end
        return out
    end

    local base = {
        {"switch", {"mode"}},
        {"case", {"a"}}, {"ch", {tag = "A"}},
        {"case", {"b"}}, {"ch", {tag = "B"}},
        {"default", {}}, {"ch", {tag = "D"}},
        {"endswitch", {}},
        {"ch", {tag = "AFTER"}},
    }

    local o1 = run_switch({ mode = "a" }, base)
    check("switch case a runs alone, then continues",
          table.concat(o1, ",") == "A,AFTER")

    local o2 = run_switch({ mode = "b" }, base)
    check("switch case b runs alone (no fall-through)",
          table.concat(o2, ",") == "B,AFTER")

    local o3 = run_switch({ mode = "z" }, base)
    check("switch default fallback",
          table.concat(o3, ",") == "D,AFTER")

    local o4 = run_switch({}, base)
    check("switch missing var falls back to default",
          table.concat(o4, ",") == "D,AFTER")

    local noDefault = {
        {"switch", {"mode"}},
        {"case", {"a"}}, {"ch", {tag = "A"}},
        {"endswitch", {}},
        {"ch", {tag = "AFTER"}},
    }
    local o5 = run_switch({ mode = "q" }, noDefault)
    check("switch no-match-no-default skips body",
          table.concat(o5, ",") == "AFTER")

    -- Nested switch: inner cases must not leak into outer flow
    local nested = {
        {"switch", {"outer"}},
        {"case", {"1"}},
        {"switch", {"inner"}},
        {"case", {"x"}}, {"ch", {tag = "IX"}},
        {"endswitch", {}},
        {"ch", {tag = "O1"}},
        {"endswitch", {}},
        {"ch", {tag = "END"}},
    }
    local o6 = run_switch({ outer = "1", inner = "x" }, nested)
    check("nested switch inner match stays in inner body",
          table.concat(o6, ",") == "IX,O1,END")
    local o7 = run_switch({ outer = "1", inner = "z" }, nested)
    check("nested switch inner no-match skips only inner body",
          table.concat(o7, ",") == "O1,END")
end

-- 10. Same-name nested [for] (round 63): the inner loop's endfor must not
--     clear the OUTER loop's counter mark — previously the outer loop
--     re-initialized its counter on re-entry and never terminated.
do
    local toks = {
        { "for", { var = "i", start = "1", ["end"] = "2" } },
        { "for", { var = "i", start = "1", ["end"] = "2" } },
        { "ch", { tag = "IN" } },
        { "endfor", {} },
        { "ch", { tag = "OUT" } },
        { "endfor", {} },
    }
    local ctx = make_ctx()
    local co = coroutine.create(function() scheduler.run(ctx, toks) end)
    local n = 0
    while coroutine.status(co) ~= "dead" and n < 1000 do
        n = n + 1
        coroutine.resume(co)
    end
    local tags = {}
    for _, d in ipairs(ctx.dispatched) do
        tags[#tags + 1] = d.params and d.params.tag or d.cmd
    end
    check("nested same-name for terminates",
        n < 1000 and coroutine.status(co) == "dead")
    -- Both loops share the f.i counter (same-name is an anti-pattern): the
    -- inner loop consumes the shared counter, so the outer runs once and
    -- ends at 3+1=4 instead of hanging forever (the pre-round-63 bug).
    check("nested same-name for shared counter",
        table.concat(tags, ",") == "IN,IN,OUT",
        table.concat(tags, ","))
    check("nested same-name for final counter", ctx.f.i == 4,
        tostring(ctx.f.i))
end

-- 19. [goto] is a KAG3 alias of [jump] (round 76): intra-scene label jump
do
    -- same semantics as the [jump] test (section 4): goto skips the
    -- intermediate command and lands on the label
    local ctx = make_ctx()
    ctx.dispatched = {}
    local toks = {
        {"goto", {storage = "L1"}}, {"bg", {file = "skip.jpg"}},
        {"label", {name = "L1"}}, {"bg", {file = "jumped.jpg"}},
    }
    run_in_coro(ctx, toks)
    local found = false
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.file == "jumped.jpg" then found = true end
    end
    check("goto aliases jump (intra-scene)", found)

    -- bare [goto *L1] form also lands
    local ctx2 = make_ctx()
    ctx2.dispatched = {}
    local toks2 = {
        {"ch", {text = "A"}}, {"goto", {target = "*L1"}},
        {"ch", {text = "SKIPPED"}},
        {"label", {name = "L1"}}, {"ch", {text = "B"}},
    }
    run_in_coro(ctx2, toks2)
    local texts = {}
    for _, d in ipairs(ctx2.dispatched) do
        if d.cmd == "ch" then texts[#texts + 1] = d.params and d.params.text end
    end
    check("goto bare *label skips to label",
        table.concat(texts, ",") == "A,B",
        table.concat(texts, ","))

    -- missing target warns but does not crash (jump parity)
    local okG = pcall(run_in_coro, make_ctx(), {{"goto", {}}})
    check("goto missing target no-crash", okG)
end


-- ---------------------------------------------------------------------------
-- 20. Control-flow edge depth (G9): nested-loop targeting, [until] worst
--     case, [for] numeric/step/fractional edges, [while] mid-body flip,
--     and [goto] into a loop body. Each assertion locks OBSERVED behavior
--     (the scheduler implementation is authoritative).
-- ---------------------------------------------------------------------------
local function collect_tags(ctx)
    local o = {}
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.tag then o[#o + 1] = d.params.tag end
    end
    return table.concat(o, ",")
end

-- Helpers: run with a frame cap so a mis-locked loop-guard assertion fails
-- the "no runaway" check instead of hanging the suite forever.
local function run_in_coro_capped(ctx, tokens, maxf)
    local co = coroutine.create(function() scheduler.run(ctx, tokens) end)
    local n = 0
    local M = maxf or 2000
    while coroutine.status(co) ~= "dead" and n < M do
        n = n + 1
        coroutine.resume(co, 16)
    end
    return n
end

-- 20a. Nested [switch] at depth 3 inside a case body: the inner switch gets
--      its own stack entry, so its endswitch pops the INNER entry while the
--      outer taken-case flag stays live; later outer cases stay skipped.
do
    local depth3 = {
        {"switch", {"l1"}}, {"case", {"1"}},
            {"switch", {"l2"}}, {"case", {"2"}},
                {"switch", {"l3"}}, {"case", {"x"}}, {"ch", {tag = "XXX"}},
                {"endswitch", {}},
                {"ch", {tag = "L2BODY"}},
            {"endswitch", {}},
            {"ch", {tag = "L1BODY"}},
        {"endswitch", {}},
        {"ch", {tag = "AFTER"}},
    }
    local c1 = make_ctx(); c1.variables = { l1 = "1", l2 = "2", l3 = "x" }
    run_in_coro_capped(c1, depth3, 200)
    check("G9 depth-3 switch path runs all matched bodies",
        collect_tags(c1) == "XXX,L2BODY,L1BODY,AFTER")

    local c2 = make_ctx(); c2.variables = { l1 = "1", l2 = "2", l3 = "z" }
    run_in_coro_capped(c2, depth3, 200)
    check("G9 depth-3 inner no-match skips only innermost body",
        collect_tags(c2) == "L2BODY,L1BODY,AFTER")
end

-- 20b. [break]/[continue] target the INNERMOST loop.
-- 20b-i. [break] in a [while] nested inside a [for]: the inner while's
--        break leaves only that while; the outer [for] continues its range.
do
    local ctx = make_ctx()
    run_in_coro_capped(ctx, {
        {"for", {var = "i", start = "1", ["end"] = "2"}},
            {"while", {exp = "1 == 1"}},
                {"break", {}},
                {"ch", {tag = "NEVER"}},
            {"endwhile", {}},
            {"ch", {tag = "FBODY"}},
        {"endfor", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 break in while-inside-for targets inner while",
        collect_tags(ctx) == "FBODY,FBODY,AFTER", collect_tags(ctx))
    check("G9 break leaves outer for counter at end", ctx.f.i == 3)
end

-- 20b-ii. [continue] in a [for] nested inside a [while]: the continue
--         skips the for body's tail, the for still completes its range, and
--         the outer while runs its next body token (here: flips go to false).
do
    local ctx = make_ctx(); ctx.f.go = 1
    run_in_coro_capped(ctx, {
        {"while", {exp = "f.go"}},
            {"for", {var = "j", start = "1", ["end"] = "3"}},
                {"continue", {}},
                {"ch", {tag = "NEVER"}},
            {"endfor", {}},
            {"iscript", {body = "ctx.f.go = false"}},
        {"endwhile", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 continue in for-inside-while skips tail, for completes",
        collect_tags(ctx) == "AFTER", collect_tags(ctx))
    check("G9 continue inner for counter completes (ends past end="..tostring(ctx.f.j)..")", ctx.f.j == 4)
    check("G9 continue outer while then exits", ctx.f.go == false)
end

-- 20b-iii. [break] inside a [switch] nested in a [while]: the break has NO
--          loop entry on the switch (switch is not a loop) so it targets the
--          surrounding [while] and exits it at the matching case.
do
    local ctx = make_ctx(); ctx.f.n = 0
    run_in_coro_capped(ctx, {
        {"while", {exp = "f.n < 5"}},
            {"switch", {exp = "f.n"}},
                {"case", {"2"}}, {"break", {}},
            {"endswitch", {}},
            {"iscript", {body = "ctx.f.n = ctx.f.n + 1"}},
        {"endwhile", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 break in switch-inside-while exits the while",
        collect_tags(ctx) == "AFTER", collect_tags(ctx))
    check("G9 break in switch-inside-while stopped at matching case",
        ctx.f.n == 2)
end

-- 20b-iv. [break] inside a [switch] nested in a [for]: breaks the [for].
do
    local ctx = make_ctx()
    run_in_coro_capped(ctx, {
        {"for", {var = "i", start = "1", ["end"] = "4"}},
            {"switch", {exp = "f.i"}},
                {"case", {"3"}}, {"break", {}},
            {"endswitch", {}},
            {"ch", {tag = "FB"}},
        {"endfor", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 break in switch-inside-for exits the for",
        collect_tags(ctx) == "FB,FB,AFTER", collect_tags(ctx))
    check("G9 break in switch-inside-for stops before end", ctx.f.i == 3)
end

-- 20c. [until]: a huge timeout with an immediately-true condition must
--      exit without waiting (no per-frame polling).
do
    local ctx = make_ctx()
    ctx.f.ok = 1
    local n = run_until(ctx,
        { {"until", { exp = "f.ok == 1", timeout = 6000000 }},
          {"ch", { text = "done" }} }, 10)
    check("G9 until huge timeout + true exits without waiting",
        n == 3 and #ctx.dispatched == 1)
end

-- 20d. [for] with fractional start/step/end: the float step accumulates in
--      the counter and the loop count is exact (inclusive of end), not
--      clipped by integer truncation.
do
    -- start 0, end 1, step 0.5 -> bodies at 0, 0.5, 1.0 (3 iterations)
    local c = make_ctx()
    run_in_coro_capped(c, {
        {"for", {var = "i", start = "0", ["end"] = "1", step = "0.5"}},
            {"ch", {tag = "B"}},
        {"endfor", {}},
        {"ch", {tag = "A"}},
    }, 200)
    check("G9 for float 0.5 step runs exact 3 bodies",
        collect_tags(c) == "B,B,B,A", collect_tags(c))
    check("G9 for float 0.5 step counter numeric past end",
        type(c.f.i) == "number" and c.f.i > 1)

    -- start 0, end 0.3, step 0.1 -> bodies at 0, 0.1, 0.2 (3 iterations)
    local c2 = make_ctx()
    run_in_coro_capped(c2, {
        {"for", {var = "i", start = "0", ["end"] = "0.3", step = "0.1"}},
            {"ch", {tag = "B"}},
        {"endfor", {}},
        {"ch", {tag = "A"}},
    }, 200)
    check("G9 for float 0.1 step runs exact 3 bodies",
        collect_tags(c2) == "B,B,B,A", collect_tags(c2))
end

-- 20e. [for] step=0: OBSERVED semantics -- the scheduler clamps a zero step
--      to 1 (scheduler.lua: "if sp == 0 then sp = 1 end"), so the loop is
--      NOT rejected with an error and does NOT hang; it iterates with step 1.
--      This documents the actual contract (authoring assumption "rejected"
--      does not hold).
do
    local ctx = make_ctx()
    local n = run_in_coro_capped(ctx, {
        {"for", {var = "i", start = "1", ["end"] = "3", step = "0"}},
            {"ch", {tag = "B"}},
        {"endfor", {}},
        {"ch", {tag = "A"}},
    }, 200)
    check("G9 for step=0 clamps to 1, no hang", n < 200, "frames " .. n)
    check("G9 for step=0 iterates start..end once",
        collect_tags(ctx) == "B,B,B,A", collect_tags(ctx))
    check("G9 for step=0 counter clamped step=1 ends 4", ctx.f.i == 4)
end

-- 20f. [for] with start > end and a NEGATIVE step iterates correctly
--      (start 3, end 0, step -1 -> bodies at 3,2,1,0; inclusive of end).
do
    local ctx = make_ctx()
    run_in_coro_capped(ctx, {
        {"for", {var = "i", start = "3", ["end"] = "0", step = "-1"}},
            {"ch", {tag = "B"}},
        {"endfor", {}},
        {"ch", {tag = "A"}},
    }, 200)
    check("G9 for start>end negative step runs 4 bodies",
        collect_tags(ctx) == "B,B,B,B,A", collect_tags(ctx))
    check("G9 for start>end negative step counter past end", ctx.f.i == -1)
end

-- 20g. [while] whose condition flips FALSE mid-body: the running body
--      completes, then the re-evaluated head exits cleanly.
do
    -- boolean-false flip via iscript on the first body line
    local ctx = make_ctx(); ctx.f.keep = true
    run_in_coro_capped(ctx, {
        {"while", {exp = "keep"}},
            {"iscript", {body = "ctx.f.keep = false"}},
            {"ch", {tag = "BODY"}},
        {"endwhile", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 while flips false mid-body: body completes, then exits",
        collect_tags(ctx) == "BODY,AFTER", collect_tags(ctx))
    check("G9 while flip leaves flag false", ctx.f.keep == false)

    -- comparison flip via iscript decrement (3 decrements)
    local c2 = make_ctx(); c2.f.n = 3
    run_in_coro_capped(c2, {
        {"while", {exp = "f.n > 0"}},
            {"iscript", {body = "ctx.f.n = ctx.f.n - 1"}},
            {"ch", {tag = "BODY"}},
        {"endwhile", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 while comparison flip runs body exactly 3 times",
        collect_tags(c2) == "BODY,BODY,BODY,AFTER", collect_tags(c2))
end

-- 20h. [goto] into a loop body (same-scene label): the goto bypasses the
--      loop HEAD, so the matching end-token has no stack entry and becomes
--      a no-op. OBSERVED semantics: the loop body executes once, the
--      counter is never initialized, and execution continues past the end
--      token -- it must never CRASH or hang (documented as undefined
--      authoring, locked here as a no-crash assertion).
do
    local c1 = make_ctx(); c1.f.n = 0
    local n1 = run_in_coro_capped(c1, {
        {"goto", {target = "*L1"}},
        {"while", {exp = "f.n < 1"}},
            {"label", {name = "L1"}},
            {"ch", {tag = "BODY"}},
            {"iscript", {body = "ctx.f.n = ctx.f.n + 1"}},
        {"endwhile", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 goto into while body: no crash/hang",
        n1 < 200, "frames " .. n1)
    check("G9 goto into while body runs body once then continues",
        collect_tags(c1) == "BODY,AFTER", collect_tags(c1))

    local c2 = make_ctx()
    local n2 = run_in_coro_capped(c2, {
        {"goto", {target = "*FL"}},
        {"for", {var = "i", start = "1", ["end"] = "2"}},
            {"label", {name = "FL"}},
            {"ch", {tag = "FBODY"}},
        {"endfor", {}},
        {"ch", {tag = "AFTER"}},
    }, 200)
    check("G9 goto into for body: no crash/hang",
        n2 < 200, "frames " .. n2)
    check("G9 goto into for body runs body once then continues",
        collect_tags(c2) == "FBODY,AFTER", collect_tags(c2))
    check("G9 goto into for body never initialized counter",
        c2.f.i == nil)
end

print(string.format("\nResults: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end