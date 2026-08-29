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

-- save the REAL module before the mocks (restored at the scroll test)
local _hu_saved = package.loaded["history_ui"]

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
local okN = pcall(KAG.history, ctxN, {})
check("no-jump handler runs", okN == true)
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
-- real show() input loop (audit): scroll + exit
-- restore the REAL module: suite runs use the saved preload (sandbox
-- blocks re-require); standalone runs CLEAR the mock cache first
-- (require would otherwise return the stub)
local HistoryUI
if _hu_saved then
    HistoryUI = _hu_saved
else
    package.loaded["history_ui"] = nil
    HistoryUI = require("history_ui")
end
local be_b = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        return true end }
local layers_b2 = package.loaded["layers"]
package.loaded["layers"] = { ensure = function() return { visible = true } end,
    find = function() return nil end, set_layer_visible = function() end,
    set_z = function() end }
local backlogH = {}
for i = 1, 30 do
    backlogH[#backlogH + 1] = { text = "line " .. i, name = "s", timestamp = 0 }
end
local ctxH = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    backlog = backlogH }
local coH = coroutine.create(function() HistoryUI.show(ctxH) end)
coroutine.resume(coH)
coroutine.resume(coH)
local okH = true
for _ = 1, 25 do
    _G._GAME_KEY_DOWN = true
    local r = coroutine.resume(coH)
    if not r then okH = false break end
end
check("history scroll no crash", okH and coroutine.status(coH) == "suspended")
_G._GAME_KEY_ESC = true
local rEsc = coroutine.resume(coH)
check("history esc exits", rEsc and coroutine.status(coH) == "dead")
-- t109: native SwipeUp -> SDLK_PAGEUP -> _KAG_onKeyPageUp opens the
-- backlog/history overlay. The hook lives in kag_demo_entry.lua (not loadable
-- here -- it autostarts demo_story.ks); this test locks the same body by
-- replicating it VERBATIM (drift risk documented at the entry site).
do
    local _savedUI = package.loaded["history_ui"]
    package.loaded["history_ui"] = { show = function(c2)
        return { index = 1, name = "target", label = "line 1",
                 scene = "t.ks", token_index = 1 }
    end }
    local ctxK = { backlog = { { text = "x", name = "s", scene = "t.ks",
                                 token_index = 7 } },
                   f = {}, tf = {}, sf = {}, mp = {}, input_focus = "kag" }
    local _savedCtx = _G._CAESURA_CTX
    _G._CAESURA_CTX = ctxK
    local history_co
    local _hookPageUp = function()  -- VERBATIM: kag_demo_entry.lua _KAG_onKeyPageUp
        local ctx = _G._CAESURA_CTX
        if ctx and ctx.input_focus ~= "history" and not history_co then
            history_co = coroutine.create(function()
                require("kag.commands.system").history(ctx, {})
            end)
        end
    end
    _hookPageUp()
    check("page-up hook opens history overlay coroutine", history_co ~= nil)
    local okP, errP = coroutine.resume(history_co)
    check("page-up hook overlay runs", okP == true)
    check("page-up hook jumps to target", ctxK._pendingJump ~= nil
          and ctxK._pendingJump.index == 1)
    local coRef = history_co
    _hookPageUp()
    check("page-up hook single-flight while overlay open", history_co == coRef)
    _G._CAESURA_CTX = _savedCtx
    package.loaded["history_ui"] = _savedUI
end

-- suite hygiene: restore the saved real module (a later [history]
-- test would otherwise hit the last mock)
package.loaded["history_ui"] = _hu_saved or package.loaded["history_ui"]
package.loaded["layers"] = layers_b2
_G._CAESURA_BACKEND = be_b

print("HISTORY TESTS DONE")
