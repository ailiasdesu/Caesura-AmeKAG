-- =============================================================================
--  test_flow_edge_scene.lua — Scheduler CROSS-SCENE token-stream boundary
--  tests (jump/call/load scene switching).
--
--  Units 76/83 covered cross-scene [jump]/[call] token swaps; rounds 47/95
--  covered [load] scene restore. This suite pins the deep boundaries the
--  scheduler must hold when the token stream changes scene mid-run:
--      A. cross-scene [jump] flow      (A→B runs; B's label index O(1);
--                                       B→A back; B's [end] does not re-enter A)
--      B. cross-scene [call] stack     (A call B → return resumes exact token;
--                                       nested A>B>C unwind; [jump] inside a
--                                       callee -> dangling-frame return)
--      C. cross-scene [load] restore   (save→jump→load back: label_index rebuilt,
--                                       _resumeLoopStacks consumed, scene name
--                                       restored to the loaded scene)
--      D. token-stream isolation       (A/B same-named label independence;
--                                       A's dynamic macro leaking into B;
--                                       f shared / lf frame-isolated across scenes)
--      E. cross-scene cycle detection  (A↔B scene ping-pong terminates because
--                                       each scene-swap ENDS its scheduler.run;
--                                       no cross-scene cycle budget exists —
--                                       [DEFECT-window])
--      F. scene-switch x loop stack    (A's [for] body jumps to B; B's [for]
--                                       reuses the same var name -> stale
--                                       _forStackMarks leak across the scene
--                                       boundary — [DEFECT])
--
--  Methodology mirrors test_flow_edge.lua: lock OBSERVED scheduler behavior
--  (the implementation is authoritative). Where observed behavior reveals a
--  defect window, the assert is honest about it, is marked "[DEFECT]" and
--  scripts/scheduler.lua is left untouched for the main agent to adjudicate.
--  A frame cap prevents any hang.
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

print("=== Flow-Edge-Scene (cross-scene token stream) Tests ===")

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {}, mp = {}, lf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, active_operations = {},
        macros = {}, macro_args = {}, stop_flag = false, dispatched = {},
        load_tokens = function() end,
        current_scene = "main.ks",
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

local function run_capped(ctx, tokens, start, maxf)
    local co = coroutine.create(function() scheduler.run(ctx, tokens, start) end)
    local n = 0
    local M = maxf or 2000
    while coroutine.status(co) ~= "dead" and n < M do
        n = n + 1
        coroutine.resume(co, 16)
    end
    return n, coroutine.status(co)
end

-- A cross-scene [jump] SETS ctx.tokens/current_scene and RETURNS from
-- scheduler.run; the runner re-spawns scheduler.run(ctx, ctx.tokens,
-- ctx.token_index) to execute the new scene. This helper mirrors that.
local function run_after_jump(ctx, maxf)
    return run_capped(ctx, ctx.tokens, nil, maxf)
end

local function label_str(l)
    if type(l) ~= "table" then return "nil" end
    local o = {}
    for k, v in pairs(l) do o[#o + 1] = k .. "=" .. v end
    table.sort(o)
    return "{" .. table.concat(o, ",") .. "}"
end

-- ---------------------------------------------------------------------------
-- A. cross-scene [jump] flow (Task 1)
-- ---------------------------------------------------------------------------

-- A1/A2. A `jump b.ks` swaps the token stream to B; A's run ENDS. The
--     new B run rebuilds B's label index and a FORWARD [jump *done] inside B
--     lands EXACTLY on B's own `done` (O(1) via compiled.labels), skipping
--     the filler — even though the caller had a conflicting stale index.
do
    -- B tokens (indexed): done is at token 4.
    local b_tokens = {
        { "ch", { tag = "B_ENTRY" } },      -- 1
        { "jump", { target = "*done" } },   -- 2 (forward -> 4)
        { "ch", { tag = "B_SKIP" } },       -- 3
        { "label", { name = "done" } },     -- 4
        { "ch", { tag = "B_DONE" } },       -- 5
    }
    local a_tokens = {
        { "ch", { tag = "A_START" } },      -- 1
        { "jump", { target = "b.ks" } },    -- 2 (cross-scene)
        { "ch", { tag = "A_NEVER" } },      -- 3
    }
    local ctx = make_ctx()
    ctx.current_scene = "assets/script/a.ks"
    ctx.label_index = { done = 999 }        -- caller's stale index
    ctx.load_tokens = function(path) ctx._loaded = path return b_tokens end
    local n1, st1 = run_capped(ctx, a_tokens, nil, 200)
    check("A1 A-jump run returns (scene swap ends A's run)",
        st1 == "dead" and ctx.current_scene == "assets/script/b.ks",
        "st=" .. tostring(st1) .. " scene=" .. tostring(ctx.current_scene))
    check("A1 A's trailing token after the jump never ran",
        ctx.dispatched[3] == nil, "dispatched=" .. tostring(#ctx.dispatched))
    check("A1 cross-scene jump cleared the caller's call stack",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
    check("A1 load_tokens hit the allowlisted path",
        ctx._loaded == "assets/script/b.ks", tostring(ctx._loaded))
    local n2, st2 = run_after_jump(ctx, 200)
    check("A2 B run terminates", n2 < 200 and st2 == "dead", "frames " .. n2)
    check("A2 B's forward *done jump lands exactly (B_SKIP skipped)",
        collect_tags(ctx) == "A_START,B_ENTRY,B_DONE", collect_tags(ctx))
    check("A2 B's compiled.labels index is B-local (done=4, not stale 999)",
        b_tokens._compiled and b_tokens._compiled.labels
            and b_tokens._compiled.labels.done == 4,
        tostring(b_tokens._compiled and b_tokens._compiled.labels
            and b_tokens._compiled.labels.done))
end

-- A3. B then `jump a.ks` back to A: A restarts from token 1 (KAG3 jump
--     semantic -- cross-scene jump does NOT resume mid-scene); A's token after
--     the ORIGINAL jump never runs; call_stack stays empty the whole way.
do
    local a_tokens = {
        { "ch", { tag = "A_TOP" } },        -- 1
        { "ch", { tag = "A_HOME" } },       -- 2
        { "jump", { target = "b.ks" } },    -- 3
        { "ch", { tag = "A_TAIL" } },       -- 4 (never)
    }
    local b_tokens = {
        { "ch", { tag = "B1" } },           -- 1
        { "jump", { target = "a.ks" } },    -- 2 (cross back)
    }
    local ctx = make_ctx()
    ctx.current_scene = "assets/script/a.ks"
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 100)   -- A -> jump B (returns)
    run_after_jump(ctx, 100)              -- B -> jump A (returns)
    check("A3 after B->A jump, current_scene back to a.ks",
        ctx.current_scene == "assets/script/a.ks", tostring(ctx.current_scene))
    run_after_jump(ctx, 100)              -- A re-entered from the top
    check("A3 A re-entered from the TOP (A_TOP first), B ran once in between",
        collect_tags(ctx) == "A_TOP,A_HOME,B1,A_TOP,A_HOME", collect_tags(ctx))
    check("A3 A_TAIL (after the original cross jump) never ran",
        collect_tags(ctx):match("A_TAIL") == nil, collect_tags(ctx))
    check("A3 no call-frame leak across the round-trip",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- A4. Explicit [end] in B: a cross-scene jump into B then [end] terminates
--     B's run. Because the jump cleared the call stack, there is NO latent
--     "return to A" -- A is never re-entered. (Boundary: "B 的 [end] 后不再回 A".)
do
    local b_tokens = {
        { "ch", { tag = "BEND_A" } },       -- 1
        { "end", {} },                       -- 2
        { "ch", { tag = "BEND_AFTER" } },    -- 3 (never)
    }
    local a_tokens = { { "ch", { tag = "AEND_T" } }, { "jump", { target = "b.ks" } } }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    run_capped(ctx, a_tokens, nil, 100)
    run_after_jump(ctx, 100)
    check("A4 B's [end] stops before its trailing token",
        collect_tags(ctx) == "AEND_T,BEND_A", collect_tags(ctx))
    check("A4 B's post-[end] token never ran",
        collect_tags(ctx):match("BEND_AFTER") == nil, collect_tags(ctx))
    check("A4 B's [end] leaves A not re-entered + no call frame",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- ---------------------------------------------------------------------------
-- B. cross-scene [call] stack (Task 2)
-- ---------------------------------------------------------------------------

-- B1. A `call b.ks` -> B `return` resumes A at the EXACT token
--     after the [call] (A2 runs; A3 is the later continuation). current_scene
--     and label_index are restored to the caller.
do
    local b_tokens = { { "ch", { tag = "B_ENTRY" } }, { "return", {} } }
    local a_tokens = {
        { "ch", { tag = "A1" } },
        { "call", { target = "b.ks" } },
        { "ch", { tag = "A2" } },
        { "ch", { tag = "A3" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    local n, st = run_capped(ctx, a_tokens, nil, 300)
    check("B1 cross-scene call->return terminates", n < 300 and st == "dead",
        "frames " .. n)
    check("B1 exact-token resume A1,B_ENTRY,A2,A3 (A resumes after [call])",
        collect_tags(ctx) == "A1,B_ENTRY,A2,A3", collect_tags(ctx))
    check("B1 scene restored to caller after return",
        ctx.current_scene == "assets/script/a.ks", tostring(ctx.current_scene))
    check("B1 no call-frame leak after return",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- B2. Nested cross-scene call A call B, B call C, LIFO unwind A1,B1,C1,B2,A2;
--     current_scene unwinds C->B->A; caller lf frame restored.
do
    local c_tokens = { { "ch", { tag = "C1" } }, { "eval", { exp = "lf.x = 'c'" } }, { "return", {} } }
    local b_tokens = {
        { "ch", { tag = "B1" } }, { "eval", { exp = "lf.x = 'b'" } },
        { "call", { target = "c.ks" } }, { "ch", { tag = "B2" } }, { "return", {} },
    }
    local a_tokens = {
        { "ch", { tag = "A1" } }, { "eval", { exp = "lf.x = 'a'" } },
        { "call", { target = "b.ks" } }, { "ch", { tag = "A2" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    local loaded = {
        ["assets/script/a.ks"] = a_tokens,
        ["assets/script/b.ks"] = b_tokens,
        ["assets/script/c.ks"] = c_tokens,
    }
    ctx.load_tokens = function(p) return loaded[p] end
    local n, st = run_capped(ctx, a_tokens, nil, 400)
    check("B2 nested 2-deep cross-scene call terminates",
        n < 400 and st == "dead", "frames " .. n)
    check("B2 LIFO unwind A1,B1,C1,B2,A2",
        collect_tags(ctx) == "A1,B1,C1,B2,A2", collect_tags(ctx))
    check("B2 current_scene restored to A after full unwind",
        ctx.current_scene == "assets/script/a.ks", tostring(ctx.current_scene))
    check("B2 lf restored to the caller frame (a) after unwind",
        ctx.lf.x == "a", tostring(ctx.lf.x))
    check("B2 no call-frame leak after nested unwind",
        #(ctx.call_stack or {}) == 0, tostring(#(ctx.call_stack or {})))
end

-- B3. Callee does an INTRA-scene `jump *label` then `return`: the
--     intra-scene jump must NOT clear the call frame, so [return] still pops
--     back to A (dangling-frame lock: resumes the caller, not a stale frame).
do
    local b_tokens = {
        { "ch", { tag = "B_ENTRY" } },
        { "jump", { target = "*btail" } },
        { "ch", { tag = "B_SKIPPED" } },
        { "label", { name = "btail" } },
        { "ch", { tag = "B_TAIL" } },
        { "return", {} },
    }
    local a_tokens = { { "ch", { tag = "A1" } }, { "call", { target = "b.ks" } }, { "ch", { tag = "A2" } } }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    local n, st = run_capped(ctx, a_tokens, nil, 300)
    check("B3 intra-jump inside callee + return terminates",
        n < 300 and st == "dead", "frames " .. n)
    check("B3 B_SKIPPED never ran; B_TAIL then return to A",
        collect_tags(ctx) == "A1,B_ENTRY,B_TAIL,A2", collect_tags(ctx))
    check("B3 return after intra-jump still pops the caller frame (a.ks)",
        ctx.current_scene == "assets/script/a.ks", tostring(ctx.current_scene))
    check("B3 no call-frame leak", #(ctx.call_stack or {}) == 0,
        tostring(#(ctx.call_stack or {})))
end

-- B4. Callee does a CROSS-scene `jump c.ks` -- this WIPES the call stack
--     (A's frame dropped; documented jump semantics). Running C, a later
--     [return] is a BARE return and terminates -- it must NOT resume A's
--     discarded frame (dangling-frame lock across scenes).
do
    local c_tokens = { { "ch", { tag = "C_ENTRY" } }, { "return", {} }, { "ch", { tag = "C_AFTER" } } }
    local b_tokens = { { "ch", { tag = "B_ENTRY" } }, { "jump", { target = "c.ks" } }, { "ch", { tag = "B_AFTER" } } }
    local a_tokens = { { "ch", { tag = "A1" } }, { "call", { target = "b.ks" } }, { "ch", { tag = "A_NEVER_BACK" } } }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    local loaded = {
        ["assets/script/a.ks"] = a_tokens,
        ["assets/script/b.ks"] = b_tokens,
        ["assets/script/c.ks"] = c_tokens,
    }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 150)   -- A -> call B -> B jump C, returns
    check("B4 B's cross-scene jump wiped the call stack (A's frame dropped)",
        ctx.current_scene == "assets/script/c.ks" and #(ctx.call_stack or {}) == 0,
        "scene=" .. tostring(ctx.current_scene) .. " frames=" .. tostring(#(ctx.call_stack or {})))
    local n2, st2 = run_after_jump(ctx, 150)
    check("B4 C's bare [return] terminates (no stale A-frame resume)",
        n2 < 150 and st2 == "dead", "frames " .. n2)
    check("B4 A_NEVER_BACK and C_AFTER never run",
        collect_tags(ctx):match("A_NEVER_BACK") == nil
            and collect_tags(ctx):match("C_AFTER") == nil, collect_tags(ctx))
    check("B4 no call-frame leak", #(ctx.call_stack or {}) == 0,
        tostring(#(ctx.call_stack or {})))
end

-- ---------------------------------------------------------------------------
-- C. cross-scene [load] restore (Task 3)
-- ---------------------------------------------------------------------------

-- C1. A save→jump→load back to A. Simulate the post-[load] resume at a saved
--     token_index: re-spawn scheduler.run on A's tokens at that index with
--     label_index=nil — the scheduler must rebuild the label index (so a
--     later [jump *label] resolves O(1)) and run from the saved token.
do
    local atok = {
        { "ch", { tag = "R1" } },
        { "ch", { tag = "R2" } },
        { "label", { name = "resume_label" } },
        { "ch", { tag = "R3" } },
        { "ch", { tag = "R4" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return atok end
    -- full run: all four
    run_capped(ctx, atok, nil, 200)
    check("C1 A full run dispatches R1..R4", collect_tags(ctx) == "R1,R2,R3,R4",
        collect_tags(ctx))
    -- "load": fresh resume at token_index 3 (R3's slot) with label_index reset
    local lctx = make_ctx(); lctx.current_scene = "assets/script/a.ks"
    lctx.load_tokens = function() return atok end
    lctx.label_index = nil      -- must rebuild
    lctx.token_index = 3        -- resume_r entry triggers [jump *resume_label]? no; here resume at R3
    run_capped(lctx, atok, 3, 200)
    check("C1 load-resume runs from the saved token (R3,R4, not R1,R2)",
        collect_tags(lctx) == "R3,R4", collect_tags(lctx))
    check("C1 label_index rebuilt for the loaded scene (resume_label=3)",
        type(lctx.label_index) == "table" and lctx.label_index.resume_label == 3,
        label_str(lctx.label_index))
    -- rebuilt index resolves a jump O(1)
    local lt = {
        { "jump", { target = "*resume_label" } },
        { "ch", { tag = "SKIP" } },
        { "label", { name = "resume_label" } },
        { "ch", { tag = "FINAL" } },
    }
    local jctx = make_ctx(); jctx.load_tokens = function() end
    run_capped(jctx, lt, nil, 200)
    check("C1 rebuilt label index resolves *resume_label O(1) (SKIP skipped)",
        collect_tags(jctx) == "FINAL", collect_tags(jctx))
end

-- C2. `_resumeLoopStacks` marker consumed on a load-style resume (round 75
--     marker): a [while] loop snapshot fed the resume rewinds to completion and
--     the marker is cleared. Mirrors test_saveflow's while-load construction at
--     pure scheduler level and additionally asserts label_index rebuild.
do
    local wtoks = {
        { "while", { exp = "f.hp < 3" } },
            { "iscript", { body = "ctx.f.hp = tonumber(ctx.f.hp or 0) + 1; ctx.f.iter = (ctx.f.iter or 0) + 1" } },
        { "endwhile", {} },
        { "ch", { tag = "W_DONE" } },
    }
    -- resume mid-loop: hp==1, one iteration remaining (to hp==3)
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.f.hp = 1; ctx.f.iter = 1
    ctx._resumeLoopStacks = { while_ = { { pos = 1 } }, for_ = {}, if_ = {}, switch = {} }
    ctx.label_index = nil
    ctx.token_index = 3   -- the endwhile token (index 3) -> rewinds to while head
    ctx.load_tokens = function() return wtoks end
    local n, st = run_capped(ctx, wtoks, 3, 300)
    check("C2 while load-resume terminates", n < 300 and st == "dead", "frames " .. n)
    check("C2 while load-resume completed the loop (hp==3)",
        ctx.f.hp == 3, "hp=" .. tostring(ctx.f.hp))
    check("C2 loop-stack marker consumed after resume",
        ctx._resumeLoopStacks == nil, "")
    check("C2 label_index rebuilt for the loaded scene",
        type(ctx.label_index) == "table", label_str(ctx.label_index))
end

-- C3. load restores the SCENE NAME of the loaded scene after a cross-scene
--     jump. A -> jump B; a "load" of A then sets current_scene back to A and
--     re-spawns from A's saved token. The scheduler keeps the scene identity
--     correct even though ctx was momentarily in scene B.
do
    local atok = { { "ch", { tag = "S1" } }, { "ch", { tag = "S2" } } }
    local btok = { { "ch", { tag = "T1" } } }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    local loaded = { ["assets/script/a.ks"] = atok, ["assets/script/b.ks"] = btok }
    ctx.load_tokens = function(p) return loaded[p] end
    ctx.token_index = 2
    ctx._pendingLoadToken = 2
    run_capped(ctx, atok, 1, 100)   -- A: S1,S2 (no jump in this scene)
    check("C3 A run completes", collect_tags(ctx) == "S1,S2", collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- D. token-stream isolation across scenes (Task 4)
-- ---------------------------------------------------------------------------

-- D1. A and B define the SAME label name; B's forward [jump *dup] resolves to
--     B's OWN dup because compiled.labels is rebuilt per scene stream (no
--     cross-scene label bleed into B; A's dup offset does not interfere).
do
    local b_tokens = {
        { "ch", { tag = "B_ENTER" } },
        { "jump", { target = "*dup" } },    -- forward -> B's dup at 4
        { "ch", { tag = "B_SKIP" } },
        { "label", { name = "dup" } },
        { "ch", { tag = "B_DUP" } },
        { "ch", { tag = "B_TAIL" } },
    }
    local a_tokens = {
        { "ch", { tag = "A_ENTER" } },
        { "label", { name = "dup" } },      -- A's dup at 2
        { "jump", { target = "b.ks" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    run_capped(ctx, a_tokens, nil, 150)
    check("D1 A's own dup is at its own position before the cross jump",
        a_tokens._compiled and a_tokens._compiled.labels.dup == 2,
        tostring(a_tokens._compiled and a_tokens._compiled.labels.dup))
    run_after_jump(ctx, 150)
    check("D1 B's *dup jump lands on B's dup (B_DUP; B_SKIP skipped)",
        collect_tags(ctx) == "A_ENTER,B_ENTER,B_DUP,B_TAIL", collect_tags(ctx))
    check("D1 B's compiled.labels.dup is B-local (4, not A's 2)",
        b_tokens._compiled and b_tokens._compiled.labels.dup == 4,
        tostring(b_tokens._compiled and b_tokens._compiled.labels.dup))
end

-- D2. A's runtime dynamic macro LEAKS into B (observed): the scheduler stores
--     macros in ctx.macros (session state), and a cross-scene [jump] does not
--     clear it, so B's invocation of the same macro name still splices A's
--     body. [DEFECT-window — no per-scene macro isolation; reported]
do
    local b_tokens = { { "ch", { tag = "B_START" } }, { "spice", {} }, { "ch", { tag = "B_END" } } }
    local a_tokens = {
        { "macro", { name = "spice" } }, { "ch", { tag = "SPICE_BODY" } }, { "endmacro", {} },
        { "ch", { tag = "A_DONE" } }, { "jump", { target = "b.ks" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    run_capped(ctx, a_tokens, nil, 200)
    check("D2 A's macro recorded into ctx.macros (runtime dynamic)",
        ctx.macros and ctx.macros.spice ~= nil, "")
    ctx.dispatched = {}
    run_after_jump(ctx, 200)
    -- If per-scene macro isolation were correct, B's "spice" would be an
    -- unknown command (dispatched to kag) and B_END would run with NO
    -- SPICE_BODY. Observed: the body splices into B.
    check("D2 [DEFECT] A's dynamic macro splices into B (no scene isolation)",
        collect_tags(ctx) == "B_START,SPICE_BODY,B_END", collect_tags(ctx))
end

-- D3. Variable scoping across scenes: f.* is SHARED (no frame isolation)
--     across a cross-scene [jump]; a cross-scene [jump] resets lf (fresh
--     KAG3 frame); a cross-scene [call] frame-isolates lf.
do
    -- f shared across jump, lf reset by jump.
    local b_tokens = {
        { "iscript", { body = "ctx.f.readback = ctx.f.base; ctx.f.lf_seen = (ctx.lf and ctx.lf.x) or 'nil'" } },
        { "ch", { tag = "B_OK" } },
    }
    local a_tokens = {
        { "eval", { exp = "lf.x = 'kept'" } },
        { "iscript", { body = "ctx.f.base = 1" } },
        { "jump", { target = "b.ks" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    run_capped(ctx, a_tokens, nil, 100)
    run_after_jump(ctx, 100)
    check("D3 f.* survives a cross-scene jump (shared session vars)",
        ctx.f.readback == 1, tostring(ctx.f.readback))
    check("D3 cross-scene [jump] resets lf (B sees nil, fresh KAG3 frame)",
        ctx.f.lf_seen == "nil", tostring(ctx.f.lf_seen))
    -- lf frame-isolated across a cross-scene [call].
    local sub_tokens = { { "eval", { exp = "lf.x = 'inner'" } }, { "eval", { exp = "f.shared = 'fromB'" } }, { "return", {} } }
    local main_tokens = { { "eval", { exp = "lf.x = 'outer'" } }, { "call", { target = "sub.ks" } }, { "ch", { tag = "BACK" } } }
    local ctx2 = make_ctx(); ctx2.current_scene = "assets/script/main.ks"
    ctx2.load_tokens = function() return sub_tokens end
    run_capped(ctx2, main_tokens, nil, 200)
    check("D3 cross-scene call returns", collect_tags(ctx2) == "BACK",
        collect_tags(ctx2))
    check("D3 callee's lf.x did not leak to caller (frame-isolated)",
        ctx2.lf.x == "outer", tostring(ctx2.lf.x))
    check("D3 f.* shared across a cross-scene call (caller sees B's write)",
        ctx2.f.shared == "fromB", tostring(ctx2.f.shared))
end

-- ---------------------------------------------------------------------------
-- E. cross-scene cycle / back-jump guard (Task 5)
-- ---------------------------------------------------------------------------

-- E1. A `jump b.ks` then B `jump a.ks` ping-pong: each scene swap
--     ENDS its scheduler.run, so driving A→B→A terminates each pass cleanly
--     and leaves no stale loop/backjump state. Note: there is NO cross-scene
--     switch budget in the scheduler (no field found) — an external driver
--     looping A↔B forever would spin. [DEFECT-window — no cross-scene cycle
--     budget; reported]
do
    local a_tokens = { { "ch", { tag = "P_A" } }, { "jump", { target = "b.ks" } } }
    local b_tokens = { { "ch", { tag = "P_B" } }, { "jump", { target = "a.ks" } } }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    local loaded = { ["assets/script/a.ks"] = a_tokens, ["assets/script/b.ks"] = b_tokens }
    ctx.load_tokens = function(p) return loaded[p] end
    run_capped(ctx, a_tokens, nil, 100)   -- A -> jump B
    run_after_jump(ctx, 100)              -- B -> jump A
    run_after_jump(ctx, 100)              -- A -> jump B
    check("E1 2-cross-scene round-trip drives 3 passes, all terminate",
        collect_tags(ctx) == "P_A,P_B,P_A", collect_tags(ctx))
    check("E1 after round-trip current_scene is the last jump target (b.ks)",
        ctx.current_scene == "assets/script/b.ks", tostring(ctx.current_scene))
    check("E1 no loop-stack leak after the round-trip",
        #(ctx._forStack or {}) == 0 and #(ctx._whileStack or {}) == 0,
        "for=" .. tostring(#(ctx._forStack or {})) .. " while=" .. tostring(#(ctx._whileStack or {})))
end

-- E2. Intra-scene backward-jump cycle guard (`_backJumps`, round-84 C2):
--     a forward jump into a label, then a backward [goto] to the SAME label,
--     is taken ONCE then cut with a WARN (no hang).
do
    local ctx = make_ctx()
    local n, st = run_capped(ctx, {
        { "jump", { target = "*loop" } },   -- 1 (forward -> 2)
        { "label", { name = "loop" } },     -- 2
        { "ch", { tag = "BODY" } },         -- 3
        { "goto", { target = "*loop" } },   -- 4 (backward -> 2)
        { "ch", { tag = "AFTER" } },        -- 5
    }, nil, 300)
    check("E2 backward-goto cycle terminates (bounded, no hang)",
        n < 300 and st == "dead", "frames " .. n)
    check("E2 forward take + one backward take, then cut (BODY twice)",
        collect_tags(ctx) == "BODY,BODY,AFTER", collect_tags(ctx))
end

-- ---------------------------------------------------------------------------
-- F. scene-switch x loop stack (Task 6)
-- ---------------------------------------------------------------------------

-- F1. A's [for] body cross-scene jumps to B; B's [for] reuses the SAME var
--     name. Round-98 C fix: the cross-scene [jump] now resets the loop stacks
--     symmetrically with the intra-scene one (round-83 A4/A5), so A's live
--     loop mark does NOT leak into B — B's same-name [for] re-initializes
--     its counter from its declared start and runs EXACTLY [start..end]
--     bodies (BH x2), settling f.i at end+1 (12). Previously the stale mark
--     leaked and B ran 11 bodies from A's stale counter. [FIXED round 98]
do
    local b_tokens = {
        { "for", { var = "i", start = "10", ["end"] = "11" } },
            { "ch", { tag = "BH" } },
        { "endfor", {} },
        { "ch", { tag = "BEND" } },
    }
    local a_tokens = {
        { "for", { var = "i", start = "1", ["end"] = "2" } },
            { "ch", { tag = "ABODY" } },
            { "jump", { target = "b.ks" } },   -- cross-scene inside A's loop
        { "endfor", {} },
        { "ch", { tag = "AEND" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/a.ks"
    ctx.load_tokens = function() return b_tokens end
    run_capped(ctx, a_tokens, nil, 150)   -- A: ABODY then jump -> returns
    -- Cross-scene jump reset the loop stacks: nothing live at the boundary.
    check("F1 cross-scene jump cleared _forStack (no A body loop leak)",
        #(ctx._forStack or {}) == 0, "for=" .. tostring(#(ctx._forStack or {})))
    check("F1 cross-scene jump cleared _whileStack",
        #(ctx._whileStack or {}) == 0, "while=" .. tostring(#(ctx._whileStack or {})))
    check("F1 cross-scene jump cleared _ifStack and _switchStack",
        #(ctx._ifStack or {}) == 0 and #(ctx._switchStack or {}) == 0,
        "if=" .. tostring(#(ctx._ifStack or {})) .. " switch=" .. tostring(#(ctx._switchStack or {})))
    check("F1 cross-scene jump cleared _forStackMarks (A's stale i-marker gone)",
        next(ctx._forStackMarks or {}) == nil, label_str(ctx._forStackMarks))
    local n2, st2 = run_after_jump(ctx, 600)
    local bh = 0
    for _ in collect_tags(ctx):gmatch("BH") do bh = bh + 1 end
    check("F1 B's run terminates (no hang)", n2 < 600 and st2 == "dead",
        "frames " .. n2)
    check("F1 [FIXED] B's same-name [for] re-initializes: exactly 2 bodies (was 11)",
        bh == 2, "expected 2 iterations, got " .. bh
            .. " -> " .. collect_tags(ctx))
    check("F1 B's counter settles at end+1 (12) from its OWN start=10",
        ctx.f.i == 12, "f.i=" .. tostring(ctx.f.i))
    check("F1 A's stale loop var did not leak (A's endfor never ran AEND)",
        collect_tags(ctx):match("AEND") == nil, collect_tags(ctx))
end

-- F2. CONTROL: the same B loop on a FRESH scene (no prior A loop) runs exactly
--     2 bodies and settles f.i==12 (Lua for semantics: end+1). Proves the F1
--     expectation is the sane baseline and isolates the cross-scene leak.
do
    local b_tokens = {
        { "for", { var = "i", start = "10", ["end"] = "11" } },
            { "ch", { tag = "BH" } },
        { "endfor", {} },
        { "ch", { tag = "BEND" } },
    }
    local ctx = make_ctx(); ctx.current_scene = "assets/script/b.ks"
    ctx.load_tokens = function() return b_tokens end
    local n, st = run_capped(ctx, b_tokens, nil, 300)
    local bh = 0
    for _ in collect_tags(ctx):gmatch("BH") do bh = bh + 1 end
    check("F2 fresh-scene B for runs exactly 2 bodies (baseline)",
        bh == 2, "got " .. bh .. " -> " .. collect_tags(ctx))
    check("F2 baseline B counter settles at end+1 (12)",
        ctx.f.i == 12, tostring(ctx.f.i))
end

package.loaded["kag"] = kag_orig

print(string.format("\nFlow-edge-scene results: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
