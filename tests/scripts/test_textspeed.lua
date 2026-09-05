-- test_textspeed.lua — [textspeed]/[cps]/[pt] write-through to the
-- typewriter reveal chain (round 71 deep verification).
--
-- The REAL read point for typewriter speed is kag_runner.update():
--   ctx.reveal.elapsed += delta_ms; shown = min(total, floor(elapsed/speed))
-- drives kag.text_scene reveal_chars. [ch]/[text] create ctx.reveal at
-- utf8-char count; [textspeed]/[cps] set ctx.text_speed (ms/char = floor(1000/cps))
-- and ctx.cps; [pt speed=] sets ctx.text_speed directly.
--
-- Part 1 drives the handlers in isolation (KAG + schema coercion).
-- Part 2 drives the REAL kag_runner: writes a temp .ks, start()s it, and
-- advances update() frames to verify per-char reveal, long-text counts,
-- the speed floor, and the skip/auto instant-reveal paths.
--
-- NOTE: each scenario writes its OWN temp scene file — flow.load_scene caches
-- by path under the shared flow module, so a reused path would serve the first
-- scenario's tokens to later ones.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")
local S = require("kag.schema")

-- ── Part 1: handler write-through --------------------------------------------
do
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    pcall(KAG.textspeed, ctx, S.coerce("textspeed", { cps = 60 }, ctx))
    check("textspeed cps=60 text_speed=16", ctx.text_speed == math.floor(1000 / 60))
    check("textspeed cps=60 cps=60", ctx.cps == 60)

    local bare = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    pcall(KAG.cps, bare, S.coerce("cps", { [1] = "50" }, bare))
    local named = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    pcall(KAG.textspeed, named, S.coerce("textspeed", { cps = 50 }, named))
    check("cps 50 bare == textspeed cps=50 (speed)",
        bare.text_speed == named.text_speed and bare.text_speed == 20)
    check("cps 50 bare == textspeed cps=50 (cps)",
        bare.cps == named.cps and bare.cps == 50)

    local pt = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    pcall(KAG.pt, pt, S.coerce("pt", { speed = 8 }, pt))
    local fast = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
    pcall(KAG.textspeed, fast, S.coerce("textspeed", { cps = 120 }, fast))
    check("pt speed=8 -> 8 ms/char", pt.text_speed == 8)
    check("textspeed cps=120 -> 8 ms/char", fast.text_speed == math.floor(1000 / 120))
    check("pt speed=8 == textspeed cps=120 read point", pt.text_speed == fast.text_speed)
end

-- ── Part 2: real kag_runner typewriter chain ----------------------------------
local seq = 0
-- Write a unique temp scene, load a FRESH kag_runner (kag_co/ctx are module
-- upvalues), start it. Returns runner + ctx + text_scene + ok.
local function launch(src)
    seq = seq + 1
    local path = "tests/scripts/_ts_round" .. seq .. ".ks"
    local f = assert(io.open(path, "w"))
    f:write(src)
    f:close()
    local krf = assert(io.open("scripts/kag_runner.lua", "r"))
    local krsrc = krf:read("*a")
    krf:close()
    local kr = assert(load(krsrc, "=kag_runner"))()
    local ok = kr.start(path)
    os.remove(path)
    return kr, kr.get_ctx(), require("kag.text_scene"), ok
end
local function reveal_chars(ts, ctx)
    return tostring(ts.get_state(ctx).reveal_chars)
end

-- [textspeed cps=60] then [ch] 10-char line: reveal matches the count and
-- advances at 16 ms/char. The first update() resumes into [ch] (creating
-- ctx.reveal); subsequent updates accumulate elapsed and floor to chars.
do
    local kr, ctx, ts, ok = launch(
        '[textspeed cps=60]\n[ch name="A" text="ABCDEFGHIJ"]\n')
    check("start ok", ok == true)
    check("textspeed wrote text_speed=16", ctx.text_speed == math.floor(1000 / 60))
    check("textspeed wrote cps=60", ctx.cps == 60)
    local r1 = kr.update(0.001)   -- resume into [ch]: reveal created, 0 chars
    check("update creates reveal", ctx.reveal ~= nil and ctx.reveal.total == 10)
    check("reveal starts empty", reveal_chars(ts, ctx) == "0")
    check("ch is waiting for input", ctx.waiting_input == true)
    -- 3 frames of 16ms -> elapsed 48ms -> floor(48/16) == 3 chars
    kr.update(0.016); kr.update(0.016); kr.update(0.016)
    check("3x16ms reveals 3 chars", reveal_chars(ts, ctx) == "3")
    check("elapsed accumulates", math.floor(ctx.reveal.elapsed) == 48)
    -- a big single frame overshoots the total but is clamped to reveal.total
    kr.update(0.5)
    check("overshoot clamped to total", reveal_chars(ts, ctx) == "10")
end

-- [textspeed cps=120] then a long [ch] (26 chars): read point 8 ms/char, the
-- reveal.total is the exact utf8 plain length, and the count reaches 26.
do
    local long = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"  -- 26 chars
    local kr, ctx, ts, ok = launch(
        '[textspeed cps=120]\n[ch name="B" text="' .. long .. '"]\n')
    check("long start ok", ok == true)
    check("cps=120 -> 8 ms/char", ctx.text_speed == 8)
    kr.update(0.001)              -- resume into [ch]
    check("long reveal total = 26", ctx.reveal ~= nil and ctx.reveal.total == 26)
    -- 26 chars * 8 ms = 208 ms to fully reveal; 13 frames of 16ms = 208ms
    for i = 1, 13 do kr.update(0.016) end
    check("long fully revealed at 208ms", reveal_chars(ts, ctx) == "26")
    check("long elapsed exact", math.floor(ctx.reveal.elapsed) == 208)
end

-- skip mode: typewriter completes instantly (skip bypasses the per-char
-- accumulation and sets reveal_chars = total in update()).
do
    local kr, ctx, ts = launch(
        '[textspeed cps=60]\n[ch name="C" text="SKIPME"]\n')
    kr.update(0.001)              -- create reveal, show 0 chars
    check("skip reveal bases total", reveal_chars(ts, ctx) == "0")
    ctx.skip_mode = true
    kr.update(0.016)
    check("skip reveals instantly to total", reveal_chars(ts, ctx) == "6")
    check("skip advanced past the line", ctx.waiting_input == false)
end

-- click / auto completion: the on_click path completes the line the instant a
-- click lands mid-animation (first click reveals, does not advance) — the
-- behavior auto mode drives after its auto_delay fires on_click.
do
    local kr, ctx, ts = launch(
        '[textspeed cps=120]\n[ch name="D" text="AUTOCLICK"]\n')
    kr.update(0.001)              -- create reveal
    kr.update(0.064)              -- 64ms / 8 = 8 of 9 chars shown
    check("auto partial before click", reveal_chars(ts, ctx) == "8")
    local ok, reason = kr.on_click()   -- first click completes the line
    check("click completes reveal", reason == "revealed"
        and reveal_chars(ts, ctx) == "9")
    check("click does not advance", ctx.waiting_input == true)
    check("click returns revealed flag", ok == true)
end

-- ---------------------------------------------------------------------------
-- Part 3 (round 77): [skip]/[auto] mode, text_speed edges, voice_wait headless
-- ---------------------------------------------------------------------------

-- skip mode driven by the REAL [skip] command (not raw ctx flag): [ch] does
-- not reveal per-char; skip completes the line instantly and advances past it
-- (waiting_input cleared) within one update.
do
    local kr, ctx, ts = launch(
        '[textspeed cps=1]\n[skip]\n[ch name="SK" text="SKIPME"]\n')
    kr.update(0.001)
    check("skip cmd sets skip_mode", ctx.skip_mode == true)
    -- skip wins over the slow cps=1 (1000 ms/char) reveal: a small number of
    -- updates reveals AND advances the 6-char line without ever waiting the
    -- full 6s.  (Observed: the reveal is created on the second update and the
    -- skip path pins it to total and advances on the third.)
    local advanced = false
    for _ = 1, 4 do
        if not ctx.waiting_input then advanced = true; break end
        kr.update(0.016)
    end
    check("skip advances despite cps=1", advanced == true, "waiting stuck")
end

-- skip + [p]: in skip mode the runner auto-advances the page break WITHOUT a
-- user click (it fires on_click each update).  The token_index must move past
-- the [p] toward the next line (updates are consumed one click per update).
-- (existing test already showed skip completes reveal; here we lock that it
-- also flows past a click-wait page break.)
do
    local kr, ctx, ts = launch(
        '[skip]\n[ch text="LINE1"]\n[p]\n[ch text="LINE2"]\n')
    kr.update(0.001)
    kr.update(0.016)              -- first skip-update completes LINE1, hits [p]
    local idxAfterFirst = ctx.token_index
    kr.update(0.016)              -- an idle update with skip: advances past [p]
    check("skip advances past [p] across updates",
        ctx.token_index and ctx.token_index > idxAfterFirst,
        "from " .. tostring(idxAfterFirst) .. " to " .. tostring(ctx.token_index))
end

-- skip toggled OFF mid-reveal: the typewriter resumes accumulating (the speed
-- floor now drives reveal again).  Reveal is no longer pinned to total.
do
    local kr, ctx, ts = launch(
        '[textspeed cps=60]\n[ch text="ABCDEFGHIJ"]\n[wait time=60000]\n')
    kr.update(0.001)              -- create reveal, 0 chars
    ctx.skip_mode = true
    kr.update(0.016)              -- skip pins reveal to total
    check("skip pins reveal to total", reveal_chars(ts, ctx) == "10")
    ctx.skip_mode = false
    ctx.reveal.elapsed = 0
    kr.update(0.016)              -- 16ms / 16 (cps=60 -> speed 16) = 1 char
    check("skip-off resumes per-char accumulation",
        reveal_chars(ts, ctx) == "1", "reveal " .. reveal_chars(ts, ctx))
    check("skip toggle is measured before session end",
        ctx.co ~= nil and coroutine.status(ctx.co) == "suspended")
    assert(kr.stop())
end

-- text_speed cps extremes via the REAL [textspeed] command: cps=1 (slowest,
-- 1000 ms/char) reveals very slowly; cps=120 is clamped to the 8 ms/char floor.
do
    local kr, ctx, ts = launch(
        '[textspeed cps=1]\n[ch text="AB"]\n')
    kr.update(0.001)
    check("cps=1 -> 1000 ms/char", ctx.text_speed == 1000 and ctx.cps == 1)
    kr.update(0.5)                -- 500ms / 1000 = 0 chars
    check("cps=1 slow: 0 chars at 500ms", reveal_chars(ts, ctx) == "0")
    kr.update(0.5)                -- cumulative 1000ms -> 1 char
    check("cps=1 slow: 1 char at 1000ms", reveal_chars(ts, ctx) == "1")

    local k2, c2, t2 = launch(
        '[textspeed cps=120]\n[ch text="ABCDEFGHIJ"]\n')
    k2.update(0.001)
    check("cps=120 -> 8 ms/char floor", c2.text_speed == 8)
end

-- mid-reveal speed change to FASTER completes the line quickly: reveal uses
-- floor(accumulated_elapsed / speed), so once a faster speed is set the
-- already-accumulated elapsed immediately reveals text (observed clamp to
-- total).  Locking that a faster mid-line speed never decreases the count.
do
    local kr, ctx, ts = launch(
        '[textspeed cps=10]\n[ch text="ABCDEFGHIJKLMNOP"]\n')
    kr.update(0.001)
    kr.update(0.1)                -- 100ms / 100 = 1 char
    local before = tonumber(reveal_chars(ts, ctx))
    ctx.text_speed = 10           -- mid-line: switch to a faster floor
    kr.update(0.02)               -- fl(120ms/10)=12, clamped to total 16
    local after = tonumber(reveal_chars(ts, ctx))
    check("faster mid-line speed reveals more, never fewer",
        after ~= nil and before ~= nil and after >= before,
        "before " .. tostring(before) .. " after " .. tostring(after))
end

-- textspeed vs skip priority: skip WINS over a slow cps (skip bypasses the
-- reveal accumulation entirely and pins to total).
do
    local kr, ctx, ts = launch(
        '[textspeed cps=1]\n[skip]\n[ch text="SLOWSKIP"]\n')
    kr.update(0.001)
    check("skip beats slow textspeed (speed read)", ctx.text_speed == 1000)
    -- skip bypasses the reveal accumulation (reveal pinned to total) so the
    -- 8-char line advances within a few updates instead of waiting 8s.
    local advanced = false
    for _ = 1, 4 do
        if not ctx.waiting_input then advanced = true; break end
        kr.update(0.016)
    end
    check("skip beats slow textspeed (advances)", advanced,
        "waiting stuck")
end

-- auto mode: continuous page-turn — with auto_mode on and auto_delay small,
-- successive updates advance the page break AND the line without any click,
-- flowing toward the later lines.  (token_index moves strictly forward.)
do
    local kr, ctx = launch(
        '[auto mode=on]\n[ch text="ONE"]\n[p]\n[ch text="TWO"]\n[p]\n')
    check("auto mode=on sets auto_mode", ctx.auto_mode == true)
    ctx.auto_delay = 50           -- fast auto cadence (headless voice = none)
    kr.update(0.001)
    local firstIdx = ctx.token_index
    for _ = 1, 30 do kr.update(0.016) end
    check("auto advances past [p] without clicks",
        ctx.token_index and ctx.token_index > firstIdx,
        "from " .. tostring(firstIdx) .. " to " .. tostring(ctx.token_index))
end

-- bare [auto] toggles auto_mode ON from the default-off state (KAG3 bare form).
do
    local kr, ctx = launch('[auto]\n')
    check("bare [auto] toggles auto_mode on", ctx.auto_mode == true)
end

-- headless degrade: [voice_wait] with NO audio backend (audio_is_playing
-- returns false) passes through immediately — the wait loop never enters.
do
    local kr, ctx, ts = launch(
        '[voice_wait]\n[ch text="VOICEDONE"]\n')
    kr.update(0.001)
    check("voice_wait headless passes immediately",
        ctx.token_index == 2, "token_index " .. tostring(ctx.token_index))
end


if failed > 0 then os.exit(1) end
print("TEXTSPEED TESTS DONE")
