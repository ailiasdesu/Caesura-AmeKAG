-- test_history.lua — backlog bounds + [history] jump signal (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- backlog is bounded (500 default) and trims from the front
local ctx = { backlog = {}, backlog_max = 5, f = {}, tf = {}, sf = {}, mp = {},
    current_scene = "t.ks", token_index = 7, variables = {} }
local push = KAG.push_backlog
for i = 1, 8 do
    push(ctx, "spk", "line " .. i, "v" .. i .. ".ogg")
end
check("backlog bounded at 5", #ctx.backlog == 5)
check("backlog trims oldest", ctx.backlog[1].text == "line 4")
check("backlog keeps newest", ctx.backlog[5].text == "line 8")
check("backlog fields", ctx.backlog[1].name == "spk"
      and ctx.backlog[1].scene == "t.ks"
      and ctx.backlog[1].token_index == 7)

-- [history] with a jump result sets _pendingJump + stop_flag (the
-- runner's dead branch consumes it). Patch HistoryUI to return a jump.
package.loaded["history_ui"] = { show = function(c2)
    return { jump = true, scene = "scripts/demo_story.ks", index = 42 }
end }
local ctx2 = { backlog = { { text = "x" } }, f = {}, tf = {}, sf = {}, mp = {},
    variables = {}, stop_flag = false, _pendingJump = nil }
local ok = pcall(KAG.history, ctx2, {})
check("history handler runs", ok == true)
check("history jump sets pendingJump", ctx2._pendingJump ~= nil
      and ctx2._pendingJump.scene == "scripts/demo_story.ks"
      and ctx2._pendingJump.index == 42)
check("history jump sets stop_flag", ctx2.stop_flag == true)

-- negative: a show() result WITHOUT the jump gate sets no signal
package.loaded["history_ui"] = { show = function()
    return { scene = "scripts/x.ks", index = 1 }  -- no jump field
end }
local ctxN = { backlog = { { text = "x" } }, f = {}, tf = {}, sf = {}, mp = {},
    variables = {}, stop_flag = false }
pcall(KAG.history, ctxN, {})
check("no-jump result is a no-op", ctxN.stop_flag == false
      and ctxN._pendingJump == nil)

-- [history] with no backlog returns without side effects
package.loaded["history_ui"] = { show = function() return nil end }
local ctx3 = { backlog = {}, f = {}, tf = {}, sf = {}, mp = {},
    variables = {}, stop_flag = false }
pcall(KAG.history, ctx3, {})
check("empty history no-op", ctx3.stop_flag == false
      and ctx3._pendingJump == nil)

if failed > 0 then os.exit(1) end
print("HISTORY TESTS DONE")
