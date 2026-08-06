-- test_trans_behavior.lua — [trans] runtime contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Transition = require("transition")
local T = require("kag.commands.transition")
local calls = {}
local real_backend = _G._CAESURA_BACKEND
local real_transition = {
    capture_screen = Transition.capture_screen,
    cancel = Transition.cancel,
    tick = Transition.tick,
    start = Transition.start,
}
_G._CAESURA_BACKEND = { render = function(cmd, ...)
    if cmd == "submit_transition" then calls[#calls + 1] = { ... } end
    if cmd == "render_frame" then return true end
    return true end }
Transition.capture_screen = function() return { id = 11 } end
Transition.cancel = function() end
Transition.tick = function() end
Transition.start = function(fromTex, toTex, params)
    -- the real start() drives submit_transition; the mock forwards so
    -- the backend contract is still exercised
    _G._CAESURA_BACKEND.render("submit_transition", 0, fromTex.id, toTex.id, params.method, 0.0)
    return true
end
-- is_active=false: the trans wait loop exits on the first check, so the
-- coroutine completes on the first resume (submit already happened)
Transition.is_active = function() return false end

-- [trans method=wipe time=1000] submits with from/to textures
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _preloadPending = false, viewport = { width = 1280, height = 720 } }
local co = coroutine.create(function() T.trans(ctx, { method = "wipe", time = 1000 }) end)
local r1 = coroutine.resume(co)
-- is_active=false => the wait loop exits immediately; one resume runs
-- the whole handler (submit happened inside)
check("trans coroutine completes", r1 and coroutine.status(co) == "dead")
check("submit called", #calls >= 1)

-- zero/negative duration: immediate promote + no submit
calls = {}
local ctx0 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    _preloadPending = false, viewport = { width = 1280, height = 720 } }
local ok0 = pcall(T.trans, ctx0, { method = "crossfade", time = 0 })
check("zero time no hang", ok0)

_G._CAESURA_BACKEND = real_backend
Transition.capture_screen = real_transition.capture_screen
Transition.cancel = real_transition.cancel
Transition.tick = real_transition.tick
Transition.start = real_transition.start

if failed > 0 then os.exit(1) end
print("TRANS BEHAVIOR TESTS DONE")
