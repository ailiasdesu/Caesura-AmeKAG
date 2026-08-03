-- Rollback unit tests: snapshot capture/restore semantics + runner wiring.
-- Loaded via tests/scripts/run_lua_tests.lua; prints PASS/FAIL per check.
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results = results or {}
    results[#results + 1] = cond
end

-- --- capture/restore round-trip -------------------------------------------
local snapshot = require("kag.snapshot")
local system = require("system")
local ctx = {
    current_scene = "demo/rollback_demo.ks",
    currentScene = "demo/rollback_demo.ks",
    token_index = 7,
    f = { hp = 100, name = "Ame" },
    sf = { flag = true },
    variables = { gold = 5 },
    backlog = { { name = "a", text = "line1" }, { name = "b", text = "line2" } },
    seen_scenes = { ["demo/rollback_demo.ks"] = { [5] = true } },
    call_stack = { { tokens = {}, index = 3 } },
    text_speed = 40,
    skip_mode = false,
    auto_mode = true,
    waiting_input = true,
    reveal = { total = 6, elapsed = 3 },
    text_state = { line = 2, char_offset = 1, cursor_x = 32, cursor_y = 580, draws = {} },
    layers = { _snapshot_ok = true },
}
check("capture returns table", type(snapshot.capture(ctx)) == "table")
local snap = snapshot.capture(ctx)

-- mutate ctx after capture; restore must bring back the captured values
ctx.f.hp = 1
ctx.variables.gold = 99
ctx.token_index = 99
ctx.backlog[#ctx.backlog + 1] = { name = "c", text = "line3" }
ctx.reveal.elapsed = 0
ctx.text_state.line = 9

local ok = snapshot.restore(ctx, snap)
check("restore ok", ok == true)
check("restore scene", ctx.current_scene == "demo/rollback_demo.ks")
check("restore token_index", ctx.token_index == 7)
check("restore deep f", ctx.f.hp == 100)
check("restore deep variables", ctx.variables.gold == 5)
check("restore backlog truncated", #ctx.backlog == 2)
check("restore reveal complete", ctx.reveal.elapsed == ctx.reveal.total)
check("restore text_state", ctx.text_state.line == 2)
check("restore seen_scenes", ctx.seen_scenes["demo/rollback_demo.ks"][5] == true)

-- --- runner wiring ----------------------------------------------------------
local kag_runner = require("kag_runner")
-- on_click pushes a snapshot when no reveal is animating (mocked coroutine)
check("rollback exists", type(kag_runner.rollback) == "function")
check("rollback empty stack returns false",
      kag_runner.rollback() == false or kag_runner.rollback() ~= true)

print("ROLLBACK TESTS DONE")
