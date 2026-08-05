-- KAG 3.0 compatibility alias tests: Japanese tag-set commands map to
-- existing Caesura commands (differentiator vs Ren'Py/Tyrano).
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

-- Load kag fresh (aliases registered in kag.lua)
package.loaded["kag"] = nil
local KAG = require("kag")

check("KAG3 [r] aliases to [l]/[br]", type(KAG.r) == "function")
check("KAG3 [s] short wait", type(KAG.s) == "function")
check("KAG3 [delay] maps wait", type(KAG.delay) == "function")
check("KAG3 [clear] aliases [cl]", KAG.clear == KAG.cl)
check("KAG3 [ld] layer delete", type(KAG.ld) == "function")
check("KAG3 [shake] effect", type(KAG.shake) == "function")
check("KAG3 [quake] routes to vfx quake", type(KAG.quake) == "function"
      and KAG.quake ~= KAG.shake)
check("KAG3 [play] bgm", type(KAG.play) == "function")
check("KAG3 [playstop]", type(KAG.playstop) == "function")
check("KAG3 [voice]", type(KAG.voice) == "function")
check("KAG3 [bgm] = play", KAG.bgm == KAG.play)
check("KAG3 [se]", type(KAG.se) == "function")

-- Functional: [delay]/[s] run inside a coroutine (they yield per frame,
-- exactly like [wait] does -- must not raise when resumed with dt)
local function runInCoro(fn)
    local co = coroutine.create(fn)
    local ok, err = coroutine.resume(co)
    if ok and coroutine.status(co) == "suspended" then
        ok, err = coroutine.resume(co, 300)  -- feed 300ms dt
    end
    return ok, err
end
local ctx = { f = {}, sf = {} }
local ok, err = runInCoro(function() KAG.delay(ctx, { ms = 100 }) end)
check("KAG3 [delay] no-crash", ok)
local ok2 = runInCoro(function() KAG.s(ctx, {}) end)
check("KAG3 [s] no-crash", ok2)

-- Functional: [ld] hides a layer without error
local ctx3 = { f = {}, sf = {}, layers = {} }
local layers = require("layers")
local fakeNode = { visible = true, texture = "x" }
-- layers.get_layer returns from an internal map; use a plain spy via the
-- module's own layerMap if accessible, else verify no-crash with unknown name
local ok3, err3 = pcall(function() KAG.ld(ctx3, { layer = "nonexistent" }) end)
check("KAG3 [ld] no-crash unknown layer", ok3)

-- [ct] aliases [cl]; [waitforclick] blocks until click
check("KAG3 [ct] aliases [cl]", KAG.ct == KAG.cl)
check("KAG3 [waitforclick] registered", type(KAG.waitforclick) == "function")

-- Exit non-zero on any failure (runner gate).
local failed = 0
for _, ok in ipairs(results or {}) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("KAG3 COMPAT TESTS DONE")
