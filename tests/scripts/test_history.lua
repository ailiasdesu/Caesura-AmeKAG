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
-- t125: the default gesture hooks are the RUNTIME canon (scripts/kag.lua
-- setup tail, exported as KAG.gesture_defaults). Tests use the exported
-- defaults directly -- no replica, no drift, no ambient-global dependence.
-- The runtime installs them on _G only when an entry has not already
-- defined its own (first-definition-wins), so the overlay driver caveat
-- stays with the entry (see kag_demo_entry.lua).
do
    local defaults = require("kag").gesture_defaults
    check("runtime gesture defaults exported", type(defaults) == "table"
          and type(defaults.page_up) == "function"
          and type(defaults.space) == "function")

    local _savedUI = package.loaded["history_ui"]
    package.loaded["history_ui"] = { show = function(c2)
        -- Real history_ui.show returns a jump-bearing table; the history
        -- handler only sets ctx._pendingJump when result.jump is truthy.
        return { jump = true, index = 1, name = "target", label = "line 1",
                 scene = "t.ks", token_index = 1 }
    end }
    local ctxK = { backlog = { { text = "x", name = "s", scene = "t.ks",
                                 token_index = 7 } },
                   f = {}, tf = {}, sf = {}, mp = {}, input_focus = "kag" }
    local _savedCtx = _G._CAESURA_CTX
    _G._CAESURA_CTX = ctxK
    local hook = defaults.page_up
    local okHook = pcall(hook)
    check("runtime page-up default callable", okHook == true)
    check("runtime page-up default creates overlay coroutine (ctx slot)",
          type(ctxK._gesture_history_co) == "thread")
    local okP, errP = coroutine.resume(ctxK._gesture_history_co)
    check("runtime page-up default overlay runs", okP == true)
    check("runtime page-up default jumps to target", ctxK._pendingJump ~= nil
          and ctxK._pendingJump.index == 1)
    local coRef = ctxK._gesture_history_co
    pcall(hook)
    check("runtime page-up default single-flight while overlay open",
          ctxK._gesture_history_co == coRef)
    -- guard: history already open -> no new coroutine slot
    ctxK._gesture_history_co = nil
    ctxK.input_focus = "history"
    pcall(hook)
    check("runtime page-up default guarded when history open",
          ctxK._gesture_history_co == nil)
    -- t130 (M-F1): actually EXECUTE the space default with a stubbed layers
    -- module (t127 blind spot: the 10 t125 assertions only ran page_up).
    local _savedLayers = package.loaded["layers"]
    local msg = { visible = true }
    package.loaded["layers"] = { get = function(name)
        if name == "message" then return msg end
        return nil
    end }
    local okT1 = pcall(defaults.space)
    check("runtime space default toggles message layer", okT1 == true
          and msg.visible == false)
    local okT2 = pcall(defaults.space)
    check("runtime space default toggles back", okT2 == true
          and msg.visible == true)
    package.loaded["layers"] = { get = function() return nil end }
    local okT3 = pcall(defaults.space)
    check("runtime space default no-message-layer no-op", okT3 == true
          and msg.visible == true)
    package.loaded["layers"] = _savedLayers
    -- headless: no ctx -> no-op without error
    _G._CAESURA_CTX = nil
    local okHQ = pcall(defaults.page_up)
    check("runtime page-up default headless no-op", okHQ == true)

    local _savedCtx2 = _G._CAESURA_CTX
    _G._CAESURA_CTX = nil
    local okS = pcall(defaults.space)
    check("runtime space default headless no-op", okS == true)
    -- t130 (M-F1) regression lock: a layers module WITHOUT get (the t127
    -- hazard shape) must not nil-call the default space body.
    local _savedLayers2 = package.loaded["layers"]
    package.loaded["layers"] = { }
    local ctxS = { }
    _G._CAESURA_CTX = ctxS
    local okS2 = pcall(defaults.space)
    check("runtime space default no-layer no-op", okS2 == true)
    package.loaded["layers"] = _savedLayers2
    _G._CAESURA_CTX = _savedCtx2
    _G._CAESURA_CTX = _savedCtx
    package.loaded["history_ui"] = _savedUI
end

-- suite hygiene: restore the saved real module (a later [history]
-- test would otherwise hit the last mock)
package.loaded["history_ui"] = _hu_saved or package.loaded["history_ui"]
package.loaded["layers"] = layers_b2
_G._CAESURA_BACKEND = be_b

-- [t131] direct coverage for the kag_runner overlay pump paths
do
    local okR, runner = pcall(require, "kag_runner")
    check("kag_runner loads in harness", okR == true)
    if okR then
        check("pump_gesture_overlay exported", type(runner.pump_gesture_overlay) == "function")
        local c1 = {}
        local ok1 = pcall(runner.pump_gesture_overlay, c1)
        check("pump slot-nil no-op", ok1 == true and c1._gesture_history_co == nil)
        local dead = coroutine.create(function() end)
        coroutine.resume(dead)
        local c2 = { _gesture_history_co = dead, input_focus = "history" }
        runner.pump_gesture_overlay(c2)
        check("pump clears dead co", c2._gesture_history_co == nil)
        check("pump dead path keeps focus", c2.input_focus == "history")
        local boom = coroutine.create(function() error("boom") end)
        local c3 = { _gesture_history_co = boom, input_focus = "history" }
        runner.pump_gesture_overlay(c3)
        check("pump clears erroring co", c3._gesture_history_co == nil)
        check("pump error path resets focus", c3.input_focus == "kag")
    end
end

print("HISTORY TESTS DONE")
