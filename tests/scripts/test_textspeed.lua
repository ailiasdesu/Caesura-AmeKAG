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

if failed > 0 then os.exit(1) end
print("TEXTSPEED TESTS DONE")
