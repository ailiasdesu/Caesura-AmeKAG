-- Backlog unit tests: entry structure + [history] jump signaling + guards.
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

-- Mock the engine-side APIs history_ui requires (no GPU in unit tests)
_G.backend = _G.backend or {}
local mockBackend = {
    create_solid_texture = function() return { _mock = true } end,
    render_text = function() end,
    set_input_focus = function() end,
    audio_play = function() end,
}
_G.backend = mockBackend
_G.layers = _G.layers or require("layers")

-- Expose layers in package.preload-free path used by the test runner
package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path

-- Entry structure via push_backlog
local TextCommands = require("kag.commands.text")
local ctx = {
    current_scene = "scripts/demo_story.ks",
    currentScene = "scripts/demo_story.ks",
    token_index = 12,
    backlog = {},
}
TextCommands.push_backlog(ctx, "Ame", "Hello world", "voice01.ogg")
check("backlog entry pushed", #ctx.backlog == 1)
check("backlog entry fields",
      ctx.backlog[1].name == "Ame"
      and ctx.backlog[1].text == "Hello world"
      and ctx.backlog[1].voice == "voice01.ogg"
      and ctx.backlog[1].scene == "scripts/demo_story.ks"
      and ctx.backlog[1].token_index == 12)
check("backlog entry timestamp", type(ctx.backlog[1].timestamp) == "number")

-- Backlog cap
ctx.backlog_max = 3
for i = 1, 6 do
    TextCommands.push_backlog(ctx, "Ame", "line" .. i)
end
check("backlog capped at max", #ctx.backlog == 3)
check("backlog keeps newest", ctx.backlog[3].text == "line6")

-- [history] command: mock HistoryUI.show returning a jump
local SystemCommands = require("kag.commands.system")
local history_ui_path = "history_ui"
local orig_require = require
-- The system command requires history_ui; stub it to return a jump table.
package.preload[history_ui_path] = function()
    return { show = function() return { jump = true, scene = "scripts/demo_story.ks", index = 42 } end }
end
local result = SystemCommands.history(ctx, {})
check("history returns jump", type(result) == "table" and result.jump == true)
check("history sets _pendingJump", ctx._pendingJump ~= nil and ctx._pendingJump.index == 42)
check("history stops script", ctx.stop_flag == true)
package.preload[history_ui_path] = nil

-- on_click guard: the runner's ctx is a module-local upvalue, so the guard
-- cannot be exercised without a live coroutine; verify it is wired in the
-- source (it must precede the coroutine checks).
local runner_src = io.open("scripts/kag_runner.lua", "r"):read("*a")
local guard_present = runner_src:find('input_focus == "history"', 1, true) ~= nil
check("on_click history guard wired", guard_present)

-- Entry hotkey wiring: the overlay coroutine must be resumed every frame
-- (single-resume orphans it: renders one frame, then ctx.input_focus stays
-- "history" and the game soft-locks). Source-level invariant check, for
-- BOTH entry files (they duplicate the driver).
for _, entry_path in ipairs({ "scripts/kag_demo_entry.lua", "demo/entry.lua" }) do
    local entry_src = io.open(entry_path, "r"):read("*a")
    check(entry_path .. " resumes history coroutine per frame",
          entry_src:find("coroutine.resume(history_co)", 1, true) ~= nil)
    check(entry_path .. " clears history_co when dead",
          entry_src:find('coroutine.status(history_co) == "dead"', 1, true) ~= nil)
    check(entry_path .. " guards re-open while active",
          entry_src:find("not history_co", 1, true) ~= nil)
    check(entry_path .. " error path resets input_focus",
          entry_src:find('ctx.input_focus = "kag"', 1, true) ~= nil)
end

print("BACKLOG TESTS DONE")
