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

package.loaded["layers"] = layers_saved

if failed > 0 then
    print(string.format("ROLLBACK MEMORY TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("ROLLBACK MEMORY TESTS DONE (%d passed)", passed))
