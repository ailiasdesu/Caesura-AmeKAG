-- test_rollback_memory.lua — rollback snapshot memory cost regression test
-- P1-5: verifies snapshot.capture stays within a sane per-snapshot budget
-- (deep-copied variable tables are inherent rollback semantics; the
-- text_state.draws shallow-copy optimization cut 64-snapshot memory by
-- ~85% at VN scale). A regression here (e.g. reverting to deep copy, or
-- growing the per-snapshot footprint) fails loudly.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. detail) or ""))
         failed = failed + 1 end
end

local snapshot = require("kag.snapshot")

-- Mock layers (capture_snapshot must exist for snapshot.capture).
local layers_saved = package.loaded["layers"]
package.loaded["layers"] = {
    capture_snapshot = function() return {} end,
    restore_snapshot = function() end,
}

local function make_ctx(nvars, ndraws)
    local ctx = {
        current_scene = "s.ks", token_index = 42,
        call_stack = {}, seen_scenes = {}, backlog = {},
        text_state = { line = 1, char_offset = 0, opacity = 255,
                       cursor_x = 32, cursor_y = 580, draws = {} },
        reveal = { total = 20, elapsed = 20 },
        f = {}, sf = {}, tf = {}, mp = {}, variables = {},
        macros = {}, characters = {}, tokens = {},
        text_speed = 50, skip_mode = false, auto_mode = false,
        waiting_input = false,
    }
    for i = 1, nvars do ctx.f["flag_" .. i] = i end
    for i = 1, ndraws do
        ctx.text_state.draws[i] = { group = "g", r = 255, g = 255, b = 255,
                                    a = 255, typewriter = true,
                                    text = "Line " .. i }
    end
    return ctx
end

-- 1. text_state.draws is shallow-copied: the snapshot's draws array is a
--    DIFFERENT table sharing the same entries (the live state appending a
--    new draw must not appear in the snapshot).
local ctx = make_ctx(10, 100)
local snap = snapshot.capture(ctx)
check("snapshot has own draws array", snap.text_state.draws ~= ctx.text_state.draws)
check("draw entries shared (shallow copy)", snap.text_state.draws[1] == ctx.text_state.draws[1])
check("draws count preserved", #snap.text_state.draws == 100)
ctx.text_state.draws[101] = { group = "g", text = "appended" }
check("live append does not leak into snapshot", #snap.text_state.draws == 100)

-- 2. variable tables are still deep-copied (rollback isolation): mutating
--    ctx.f after capture must not affect the snapshot.
ctx.f.flag_1 = 999
check("vars deep-copied (isolation)", snap.f.flag_1 == 1)

-- 3. restore swaps the state back (draws array replaced, not merged).
local ctx2 = make_ctx(10, 100)
local snap2 = snapshot.capture(ctx2)
ctx2.text_state.draws[1] = { group = "x", text = "mutated" }
ctx2.f.flag_1 = 777
local ok = snapshot.restore(ctx2, snap2)
check("restore succeeds", ok == true)
check("restore swaps draws array", ctx2.text_state.draws[1].text == "Line 1"
      and #ctx2.text_state.draws == 100)
check("restore swaps vars", ctx2.f.flag_1 == 1)

-- 4. memory budget at typical VN scale: 64 snapshots stay under a bound.
--    Measured ~1.0 MB (15.9 KB/snap) for 200 vars + 300 draws; the bound
--    allows 3x headroom for deeper text_state/layers.
local ctx3 = make_ctx(200, 300)
for i = 1, 100 do
    ctx3.backlog[i] = { scene = "s.ks", index = i, text = "backlog" }
end
collectgarbage("collect")
local base = collectgarbage("count")
local snaps = {}
for i = 1, 64 do snaps[i] = snapshot.capture(ctx3) end
collectgarbage("collect")
local mem = collectgarbage("count") - base
check("64-snapshot stack under 3 MB budget",
      mem < 3 * 1024, string.format("%.1f KB", mem))
check("per-snapshot under 48 KB",
      (mem / 64) < 48, string.format("%.1f KB/snap", mem / 64))
print(string.format("  [mem] 64 snaps @ 200vars+300draws: %.1f KB (%.1f KB/snap)",
      mem, mem / 64))

-- 5. extreme scale still bounded (the deep-copied variable tables are the
--    dominant remaining cost; 2000 vars + 2000 draws must stay under a
--    hard cap -- measured ~5.3 MB after the shallow-copy optimization).
local ctx4 = make_ctx(2000, 2000)
collectgarbage("collect")
local base4 = collectgarbage("count")
local snaps4 = {}
for i = 1, 64 do snaps4[i] = snapshot.capture(ctx4) end
collectgarbage("collect")
local mem4 = collectgarbage("count") - base4
check("extreme scale under 12 MB cap",
      mem4 < 12 * 1024, string.format("%.1f KB", mem4))
print(string.format("  [mem] 64 snaps @ 2000vars+2000draws: %.1f KB (%.1f KB/snap)",
      mem4, mem4 / 64))

-- 6. Seen-token state grows throughout a story. Keep all 64 history entries
-- without retaining a full numeric-key hash table in each snapshot.
do
    local seen_ctx = make_ctx(0, 0)
    local flags = {}
    for i = 1, 3000 do flags[i * 3] = true end
    -- Exercise both ends of a 64-bit block and a very sparse valid index.
    flags[1], flags[63], flags[64], flags[65], flags[2147483647] = true, true, true, true, true
    seen_ctx.seen_scenes = { ["large.ks"] = flags, ["empty.ks"] = {} }
    collectgarbage("collect")
    local before = collectgarbage("count")
    local history = {}
    local started = os.clock()
    for i = 1, 64 do
        flags[10000 + i] = true
        history[i] = snapshot.capture(seen_ctx)
    end
    local capture_ms = (os.clock() - started) * 1000
    collectgarbage("collect")
    local retained = collectgarbage("count") - before
    check("64 large seen snapshots under 512 KB", retained < 512, string.format("%.1f KB", retained))
    print(string.format("  [seen] 64 snaps @ 3000 flags: %.1f KB; capture %.3f ms", retained, capture_ms))

    flags[3], flags[64], flags[9001] = nil, false, true
    seen_ctx.seen_scenes = { ["future.ks"] = { [1] = true } }
    snapshot.restore(seen_ctx, history[1])
    local restored = seen_ctx.seen_scenes["large.ks"]
    local exact = restored and seen_ctx.seen_scenes["future.ks"] == nil
        and type(seen_ctx.seen_scenes["empty.ks"]) == "table"
        and restored[1] == true and restored[63] == true and restored[64] == true
        and restored[65] == true and restored[2147483647] == true and restored[9001] == nil
        and restored[10001] == true and restored[10002] == nil
    for i = 1, 3000 do exact = exact and restored[i * 3] == true end
    local count = 0
    for _ in pairs(restored or {}) do count = count + 1 end
    exact = exact and count == 3005
    check("large seen snapshot restores exact flags despite live mutation", exact)
    restored[64], restored[10002] = false, true
    snapshot.restore(seen_ctx, history[1])
    check("restoring twice never mutates the retained seen snapshot",
        seen_ctx.seen_scenes["large.ks"][64] == true and seen_ctx.seen_scenes["large.ks"][10002] == nil)
    snapshot.restore(seen_ctx, history[64])
    check("the latest of all 64 snapshots retains its own additions",
        seen_ctx.seen_scenes["large.ks"][10064] == true)
end

-- Unusual legacy key/value shapes retain deep-copy behavior, including
-- aliases, cycles, false flags, table keys, and non-token metadata.
do
    local seen_ctx = make_ctx(0, 0)
    local key = { label = "key" }
    local shared = { [1] = true, [0] = false, [3.5] = "fractional", meta = { value = 7 },
        [key] = { value = "table-key" } }
    shared.self = shared
    seen_ctx.seen_scenes = { first = shared, second = shared, count = 2 }
    local saved = snapshot.capture(seen_ctx)
    shared.meta.value, shared[0], key.label = 99, true, "changed"
    snapshot.restore(seen_ctx, saved)
    local result = seen_ctx.seen_scenes
    check("non-token seen values and aliases survive rollback", result.first == result.second
        and result.first.self == result.first and result.count == 2 and result.first[0] == false
        and result.first[3.5] == "fractional" and result.first.meta.value == 7)
    local copied_key
    for candidate in pairs(result.first) do if type(candidate) == "table" then copied_key = candidate end end
    check("table keys retain deep-copy isolation", copied_key ~= key and copied_key.label == "key"
        and result.first[copied_key].value == "table-key")
    -- A standard-shaped graph can also contain a shared per-scene map.
    local common = { [64] = true }
    seen_ctx.seen_scenes = { first = common, second = common }
    saved = snapshot.capture(seen_ctx)
    common[64] = false
    snapshot.restore(seen_ctx, saved)
    check("standard-shaped aliases remain shared after restore",
        seen_ctx.seen_scenes.first == seen_ctx.seen_scenes.second and seen_ctx.seen_scenes.first[64] == true)

    local legacy = { scene = "legacy.ks", token_index = 1, seen_scenes = { ["legacy.ks"] = { [91] = true } } }
    snapshot.restore(seen_ctx, legacy)
    seen_ctx.seen_scenes["legacy.ks"][91] = false
    snapshot.restore(seen_ctx, legacy)
    check("old snapshots restore without the new private representation",
        seen_ctx.seen_scenes["legacy.ks"][91] == true)
end

-- Incremental private encoding must notice removals as well as additions,
-- including an equal-sized replacement and transitions to/from fallback.
do
    local seen_ctx = make_ctx(0, 0)
    local flags = { [1] = true, [64] = true, [65] = true, [4096] = true }
    seen_ctx.seen_scenes = { story = flags }
    local original = snapshot.capture(seen_ctx)
    flags[1], flags[64], flags[65], flags[4096] = nil, nil, nil, nil
    flags[8192], flags[8193], flags[8194], flags[8195] = true, true, true, true
    local replaced = snapshot.capture(seen_ctx)
    flags[8192] = false
    local unusual = snapshot.capture(seen_ctx)
    flags[8192], flags[8193] = true, nil
    local standard_again = snapshot.capture(seen_ctx)
    snapshot.restore(seen_ctx, original)
    check("earlier packed snapshots survive later block replacements",
        seen_ctx.seen_scenes.story[64] == true and seen_ctx.seen_scenes.story[8192] == nil)
    snapshot.restore(seen_ctx, replaced)
    check("equal-sized key replacement removes old flags",
        seen_ctx.seen_scenes.story[1] == nil and seen_ctx.seen_scenes.story[8192] == true)
    snapshot.restore(seen_ctx, unusual)
    check("false flags use lossless fallback", seen_ctx.seen_scenes.story[8192] == false)
    snapshot.restore(seen_ctx, standard_again)
    check("returning from fallback captures current flags exactly",
        seen_ctx.seen_scenes.story[8192] == true and seen_ctx.seen_scenes.story[8193] == nil)
end

-- Cache work buffers may retain only current flags. Holding the newest
-- snapshot must not keep the original live tables or historical empty blocks.
do
    local seen_ctx = make_ctx(0, 0)
    local flags = { [1] = true }
    seen_ctx.seen_scenes = { story = flags }
    collectgarbage("collect")
    local before = collectgarbage("count")
    local last
    for i = 1, 2000 do
        flags[(i - 1) * 64 + 1] = nil
        flags[i * 64 + 1] = true
        last = snapshot.capture(seen_ctx)
    end
    collectgarbage("collect")
    local retained = collectgarbage("count") - before
    check("removed sparse blocks do not accumulate in private cache", retained < 64,
        string.format("%.1f KB", retained))
    local original = setmetatable({ [flags] = true }, { __mode = "k" })
    seen_ctx, flags = nil, nil
    collectgarbage("collect")
    collectgarbage("collect")
    check("retained snapshots do not keep mutable source tables alive", next(original) == nil)
    local target = make_ctx(0, 0)
    snapshot.restore(target, last)
    check("snapshot remains usable after its source table is collected", target.seen_scenes.story[128001] == true)
end

-- A failed encoding must not leave a half-updated work buffer that silently
-- drops a newly seen flag on the next capture. Prior snapshots remain usable.
do
    local seen_ctx = make_ctx(0, 0)
    local flags = { [1] = true }
    seen_ctx.seen_scenes = { story = flags }
    local earlier = snapshot.capture(seen_ctx)
    flags[2] = true
    local original_pack = string.pack
    string.pack = function() error("injected seen encoding failure", 0) end
    local captured = pcall(snapshot.capture, seen_ctx)
    string.pack = original_pack
    check("encoding failure is observable without altering live flags", not captured and flags[2] == true)
    local target = make_ctx(0, 0)
    snapshot.restore(target, earlier)
    check("encoding failure cannot damage a retained snapshot", target.seen_scenes.story[1] == true
        and target.seen_scenes.story[2] == nil)
    snapshot.restore(target, snapshot.capture(seen_ctx))
    check("capture after encoding failure rebuilds exact seen flags", target.seen_scenes.story[1] == true
        and target.seen_scenes.story[2] == true)
end

package.loaded["layers"] = layers_saved

if failed > 0 then
    print(string.format("ROLLBACK MEMORY TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("ROLLBACK MEMORY TESTS DONE (%d passed)", passed))
