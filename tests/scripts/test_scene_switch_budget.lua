-- =============================================================================
--  test_scene_switch_budget.lua — Cross-scene switch budget (round 98, dead-loop
--  guard) tests.
--
--  The scheduler has no separate runner: kag_runner.lua drives scheduler.run
--  from the engine frame loop, and a cross-scene [jump]/[link] dies the
--  coroutine (the runner re-spawns it); a cross-scene [call] swaps tokens
--  inline and never returns. BEFORE this round there was no budget on how many
--  times the token stream can switch scenes in one session, so:
--      * an A<->B [jump]/[link] ping-pong spun forever under the frame loop
--        (each switch ended a run, but the runner re-spawned it), and
--      * an A<->B [call] recursion grew ctx.call_stack without bound inside a
--        single run (every cross-scene call pushed a frame; nothing bounded it).
--
--  [round 98 C] scheduler.budget_scene_switch() adds a SESSION-scoped counter
--  (ctx._sceneSwitches, reset ONLY on a fresh kag_runner.start, never per
--  scheduler.run) shared by [jump]/[call]/[link]. When it exceeds the cap the
--  switch is CUT with a WARN and fall-through (the token is skipped, execution
--  continues at the next token of the CURRENT scene) -- the same cut-and-WARN
--  shape as the round-84 _backJumps backward-jump guard.
--
--  Because tests drive scheduler.run directly (not kag_runner.start), the
--  counter starts at nil (~0). We seed ctx._sceneSwitches near the cap so the
--  budget trips in a handful of frames instead of 4096.
--
--  Methodology mirrors test_flow_edge_scene.lua: lock observed behavior; a
--  frame cap prevents any hang.
-- =============================================================================

package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local scheduler = require("scheduler")

-- Mock the kag command table so dispatches are recorded invisibly (headless,
-- no backend). Save the real one to restore at the end (mirror test_flow_edge).
local kag_orig = package.loaded["kag"]
local kmock = {}
kmock.ch = function(ctx, p)
    ctx.dispatched[#ctx.dispatched + 1] = { cmd = "ch", params = p }
end
package.loaded["kag"] = kmock

local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then
        passed = passed + 1
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s  -- %s", name, detail or ""))
    end
end

print("=== Scene-Switch Budget (cross-scene dead-loop guard) Tests ===")

-- Mirrors scheduler.lua's local (not exported). Tests seed near this value.
local SCENE_SWITCH_MAX = 4096

local function make_ctx(seed)
    return {
        f = {}, sf = {}, tf = {}, mp = {}, lf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, macro_args = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
        current_scene = "assets/script/a.ks",
        _sceneSwitches = seed or 0,
    }
end

local function collect_tags(ctx)
    local o = {}
    for _, d in ipairs(ctx.dispatched) do
        if d.params and d.params.tag then o[#o + 1] = d.params.tag end
    end
    return table.concat(o, ",")
end

local function run_capped(ctx, tokens, start, maxf)
    local co = coroutine.create(function() scheduler.run(ctx, tokens, start) end)
    local n = 0
    local M = maxf or 3000
    while coroutine.status(co) ~= "dead" and n < M do
        n = n + 1
        coroutine.resume(co, 16)
    end
    return n, coroutine.status(co)
end

local function run_after_jump(ctx, maxf)
    return run_capped(ctx, ctx.tokens, nil, maxf)
end

-- ---------------------------------------------------------------------------
-- 1. Below-budget normal cross-scene [jump] is unaffected (regression lock).
--    A->B->A 2-cross-scene round-trip drives each pass cleanly; the budget
--    counter grew but no switch was cut.
-- ---------------------------------------------------------------------------
do
    local a_tokens = { { "ch", { tag = "R_A" } }, { "jump", { target = "b.ks" } } }
    local b_tokens = { { "ch", { tag = "R_B" } }, { "jump", { target = "a.ks" } } }
    local ctx = make_ctx(0)   -- fresh budget
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 100)
    run_after_jump(ctx, 100)
    run_after_jump(ctx, 100)  -- A -> B -> A
    check("1 fresh budget: A->B->A completes, 3 passes",
        collect_tags(ctx) == "R_A,R_B,R_A", collect_tags(ctx))
    check("1 budget incremented for allowed switches (3 jumps)",
        (tonumber(ctx._sceneSwitches) or 0) >= 3,
        "switches=" .. tostring(ctx._sceneSwitches))
    check("1 no switch was cut below the cap",
        (tonumber(ctx._sceneSwitches) or 0) <= SCENE_SWITCH_MAX,
        "switches=" .. tostring(ctx._sceneSwitches))
end

-- ---------------------------------------------------------------------------
-- 2. [jump] ping-pong: when the budget is exhausted AFTER an allowed switch,
--    the NEXT cross-scene [jump] is CUT (WARN + fall-through): the scene does
--    NOT change and execution continues at the next token of the same scene.
-- ---------------------------------------------------------------------------
do
    local a_tokens = { { "ch", { tag = "K_A" } }, { "jump", { target = "b.ks" } }, { "ch", { tag = "K_A_TAIL" } } }
    local b_tokens = { { "ch", { tag = "K_B" } }, { "jump", { target = "a.ks" } }, { "ch", { tag = "K_B_TAIL" } } }
    -- Seed one below cap: first jump consumes the last unit, second is cut.
    local ctx = make_ctx(SCENE_SWITCH_MAX - 1)
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 150) -- A: K_A, jump->B consumes unit #4096 (allowed)
    run_after_jump(ctx, 150)            -- B: K_B, jump->A is CUT (budget spent)
    check("2 budget-exhausted [jump] is cut: current_scene stays b.ks",
        ctx.current_scene == "assets/script/b.ks", tostring(ctx.current_scene))
    check("2 cut falls through to B's next token (K_B_TAIL ran)",
        collect_tags(ctx):match("K_B_TAIL") ~= nil, collect_tags(ctx))
    check("2 A is never re-entered after the cut (K_A_TAIL not run)",
        collect_tags(ctx):match("K_A_TAIL") == nil, collect_tags(ctx))
    -- increment-precedes-check: the cut lands the counter at cap+1 (bounded
    -- at cap+1 -- the guard never lets it keep climbing past a cut).
    check("2 counter bounded at the cut (cap+1, not unbounded)",
        (tonumber(ctx._sceneSwitches) or 0) == SCENE_SWITCH_MAX + 1,
        "switches=" .. tostring(ctx._sceneSwitches))
    check("2 ping-pong terminated (run ended, no spin)",
        #collect_tags(ctx) <= SCENE_SWITCH_MAX, "tags=" .. #collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- 3. Cross-scene [call] recursion is bounded: an A->[call b]->[call a]->...
--    chain would grow ctx.call_stack without bound. The budget cuts the Nth
--    call: no frame is pushed, no swap, fall-through continues the current
--    scene and the existing frame unwinds normally.
-- ---------------------------------------------------------------------------
do
    local a_tokens = { { "call", { target = "b.ks" } }, { "ch", { tag = "C_A_AFTER" } } }
    local b_tokens = { { "call", { target = "a.ks" } }, { "ch", { tag = "C_B_AFTER" } } }
    local ctx = make_ctx(SCENE_SWITCH_MAX - 1)
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    local n, st = run_capped(ctx, a_tokens, nil, 400)
    check("3 bounded [call] recursion terminates (no hang)",
        n < 400 and st == "dead", "frames " .. n)
    -- A call b (consumes #4096, allowed, pushes frame{A}) -> B call a is CUT
    -- (no 2nd frame) -> B_AFTER runs -> implicit return pops A -> A_AFTER.
    -- The cut means B's [call a] never pushed a 2nd frame -- the call_stack
    -- never grew past A's single frame. After B ends its implicit return pops
    -- that one frame, so the FULLY-unwound end state is depth 0.
    check("3 no unbounded call stack (fully unwound at end, depth 0)",
        #(ctx.call_stack or {}) == 0, "depth=" .. tostring(#(ctx.call_stack or {})))
    check("3 fall-through executed B_AFTER (B continued after the cut call)",
        collect_tags(ctx):match("C_B_AFTER") ~= nil, collect_tags(ctx))
    check("3 A resumed after B (C_A_AFTER ran)",
        collect_tags(ctx):match("C_A_AFTER") ~= nil, collect_tags(ctx))
    check("3 counter bounded at the cut (cap+1)",
        (tonumber(ctx._sceneSwitches) or 0) == SCENE_SWITCH_MAX + 1,
        "switches=" .. tostring(ctx._sceneSwitches))
end

-- ---------------------------------------------------------------------------
-- 4. Cross-scene [link] below budget works; when budget is exhausted [link] is
--    CUT WITHOUT wiping layers/backlog/call_stack (no scene to link into).
-- ---------------------------------------------------------------------------
do
    -- 4a. below budget: link swaps normally.
    local b_tokens = { { "ch", { tag = "L_B" } } }
    local a_tokens = { { "link", { target = "b.ks" } } }
    local ctx = make_ctx(0)
    ctx.load_tokens = function() return b_tokens end
    ctx.layers = { { "fake" } }; ctx.backlog = { { "old" } }
    -- [link] swaps tokens INLINE (unlike [jump]) -- one run self-completes A
    -- and the linked B in the same coroutine; no re-spawn needed.
    run_capped(ctx, a_tokens, nil, 100)
    check("4a below-budget [link] switches to B (L_B ran exactly once)",
        collect_tags(ctx) == "L_B", collect_tags(ctx))
    check("4a link reset layers/backlog as designed",
        #(ctx.layers or {}) == 0 and #(ctx.backlog or {}) == 0,
        "layers=" .. #(ctx.layers or {}) .. " backlog=" .. #(ctx.backlog or {}))
end

do
    -- 4b. exhausted budget: [link] is cut and MUST NOT wipe scene state.
    local b_tokens = { { "ch", { tag = "L2_B" } } }
    local a_tokens = { { "link", { target = "b.ks" } }, { "ch", { tag = "L2_A_TAIL" } } }
    local ctx = make_ctx(SCENE_SWITCH_MAX)  -- already at cap: first link is cut
    ctx.load_tokens = function() return b_tokens end
    ctx.layers = { { "fake" } }; ctx.backlog = { { "old" } }
    run_capped(ctx, a_tokens, nil, 100)
    check("4b budget-exhausted [link] is cut: current_scene stays a.ks",
        ctx.current_scene == "assets/script/a.ks", tostring(ctx.current_scene))
    check("4b cut [link] does NOT wipe layers/backlog (unlike a real link)",
        #(ctx.layers or {}) == 1 and #(ctx.backlog or {}) == 1,
        "layers=" .. #(ctx.layers or {}) .. " backlog=" .. #(ctx.backlog or {}))
    check("4b cut [link] falls through to the next token (L2_A_TAIL ran)",
        collect_tags(ctx):match("L2_A_TAIL") ~= nil, collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- 5. Session semantics: a FRESH session (new ctx with counter 0) receives a
--    whole fresh budget -- the counter is not a global/static.
-- ---------------------------------------------------------------------------
do
    -- Reuse the exhausted-ping-pong scenario on a fresh (seed 0) ctx: it runs
    -- the full 3-pass round-trip without any cut (i.e. budget is per-session).
    local a_tokens = { { "ch", { tag = "S_A" } }, { "jump", { target = "b.ks" } } }
    local b_tokens = { { "ch", { tag = "S_B" } }, { "jump", { target = "a.ks" } } }
    local ctx = make_ctx(0)
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 100)
    run_after_jump(ctx, 100)
    run_after_jump(ctx, 100)
    check("5 fresh session gets a fresh budget (3-pass round-trip, no cut)",
        collect_tags(ctx) == "S_A,S_B,S_A" and ctx.current_scene == "assets/script/b.ks",
        collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- 6. Security: the budget guard must not bypass the path allowlist. A blocked
--    path still WARNs and does not consume the budget / does not switch.
-- ---------------------------------------------------------------------------
do
    local ctx = make_ctx(0)
    ctx.current_scene = "assets/script/a.ks"
    local evil = { { "jump", { target = "../evil.ks" } }, { "ch", { tag = "SEC_AFTER" } } }
    ctx.load_tokens = function() return { { "ch", { tag = "EVIL" } } } end
    run_capped(ctx, evil, nil, 100)
    check("6 blocked traversal path never loads (EVIL not dispatched)",
        collect_tags(ctx):match("EVIL") == nil, collect_tags(ctx))
    check("6 blocked path does not consume the switch budget",
        (tonumber(ctx._sceneSwitches) or 0) == 0,
        "switches=" .. tostring(ctx._sceneSwitches))
    check("6 blocked path falls through to next token (SEC_AFTER ran)",
        collect_tags(ctx):match("SEC_AFTER") ~= nil, collect_tags(ctx))
end

package.loaded["kag"] = kag_orig

print(string.format("\nScene-switch-budget results: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
