-- test_wait_audio.lua — [waitsound]/[waitbgm] bounded-wait contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- Behavior: a stuck backend (audio always "playing") must NOT hang --
-- the 60s cap breaks the loop. Drive the REAL handlers with a mock
-- backend that never stops. NOTE: re-require is only possible OUTSIDE
-- the suite sandbox (its require wrapper rejects un-preloaded modules),
-- so the behavior half runs standalone; the suite locks the source form.
local sandboxed = _G._SANDBOX_MODE ~= nil
local real_backend = package.loaded["backend"]
local mock = setmetatable({}, { __index = function(_, k)
    if k == "audio_is_playing" then return function() return true end end
    return function() end
end})
package.loaded["backend"] = mock
local Audio = package.loaded["kag.commands.audio"]
if not sandboxed then
    -- force re-require so the module binds the mock backend
    package.loaded["kag.commands.audio"] = nil
    Audio = require("kag.commands.audio")
end
-- (outside the sandbox the module's `local backend` captured the mock)

local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "t.ks", token_index = 1 }
if sandboxed then
    -- suite: direct call with the real backend (no _CAESURA_BACKEND ->
    -- resolve chain returns false -> immediate return, no hang)
    local okD = pcall(Audio.waitsound, ctx, {})
    check("waitsound no-backend safe", okD)
end
local co = coroutine.create(function() Audio.waitsound(ctx, {}) end)
-- feed 61s of dt in chunks -- the cap must break the loop well before
local ok = true
for _ = 1, 7000 do  -- 7000 * 16ms = 112s if it never broke
    local r, e = coroutine.resume(co, 16)
    if not r then ok = false break end
    if coroutine.status(co) == "dead" then break end
end
check("waitsound bounded (61s max)", ok and coroutine.status(co) == "dead")

local co2 = coroutine.create(function() Audio.waitbgm(ctx, {}) end)
local ok2 = true
for _ = 1, 7000 do
    local r, e = coroutine.resume(co2, 16)
    if not r then ok2 = false break end
    if coroutine.status(co2) == "dead" then break end
end
check("waitbgm bounded (61s max)", ok2 and coroutine.status(co2) == "dead")

-- source: the cap constant exists and both loops use it
local f = assert(io.open("scripts/kag/commands/audio.lua", "r"))
local src = f:read("*a")
f:close()
check("cap constant", src:find("WAIT_AUDIO_LIMIT_MS = 60000", 1, true) ~= nil)
check("waitsound bounded", src:find('audio_is_playing("se") and elapsed < WAIT_AUDIO_LIMIT_MS', 1, true) ~= nil)
check("waitbgm bounded", src:find('audio_is_playing("bgm") and elapsed < WAIT_AUDIO_LIMIT_MS', 1, true) ~= nil)

package.loaded["backend"] = real_backend

if failed > 0 then os.exit(1) end
print("WAIT AUDIO TESTS DONE")
