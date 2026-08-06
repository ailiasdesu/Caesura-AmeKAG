-- Title-demo entry tests: action routing (new -> kag_runner.start, etc).
local results = {}  -- file scope: runner shares globals
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
        results[#results + 1] = cond
end

package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
local _preload_backend = package.preload["backend"]
package.preload["backend"] = function()
    return {
        create_solid_texture = function() return { _mock = true } end,
        render_text = function() end,
        get_input_focus = function() return "kag" end,
    }
end
-- suite hygiene: the preload mock + cache clear PERSIST past this
-- file -- under the suite sandbox later require("backend") fails with
-- "not preloaded" (font/video tests died). Save both, restore at end.
-- captured BEFORE the mock below (review nit: capturing after would
-- restore the mock itself)
local _loaded_backend = package.loaded["backend"]
package.loaded["backend"] = nil

-- (suite hygiene: capture the ORIGINAL preload BEFORE the mock -- the
-- restore at the end must put back nil, not the mock itself)
local _preload_kr = package.preload["kag_runner"]

-- Mock kag_runner: record start() calls
package.preload["kag_runner"] = function()
    return {
        start = function(path) _G._mock_started = path end,
        update = function() end,
        on_click = function() return true end,
    }
end
package.loaded["kag_runner"] = nil

-- Load the entry file (module-level code runs the title coroutine creation)
local entry_src = io.open("scripts/title_demo_entry.lua", "r"):read("*a")
check("entry drives title menu", entry_src:find("TitleMenu.show", 1, true) ~= nil)
check("entry routes new game", entry_src:find('action == "new"', 1, true) ~= nil)
check("entry routes load", entry_src:find('action == "load"', 1, true) ~= nil)
check("entry routes settings", entry_src:find('action == "settings"', 1, true) ~= nil)
check("entry routes exit", entry_src:find('action == "exit"', 1, true) ~= nil)
check("entry falls back on menu error", entry_src:find("fall back to starting", 1, true) ~= nil)

-- Functional: simulate the engine_update driving a title menu that returns "new"
local loaded = loadfile("scripts/title_demo_entry.lua")
_G._CAESURA_CTX = { f = {}, sf = {}, input_focus = "kag" }
package.loaded["title_menu"] = { show = function() return "new" end }
local ok = pcall(loaded)
-- The entry's top-level code runs immediately; title_co was created; then
-- calling engine_update drives it to completion -> kag_runner.start
if ok and _G.engine_update then
    _G.engine_update(0.016)  -- first resume: menu yields? no -- returns "new" immediately
    _G.engine_update(0.016)
end
check("new game starts demo", _G._mock_started == "scripts/demo_story.ks")

-- exit action: engine.quit called
package.loaded["title_menu"] = { show = function() return "exit" end }
_G._mock_quit = false
_G._CAESURA_ENGINE = { quit = function() _G._mock_quit = true end }
_G._mock_started = nil
package.loaded["title_demo_entry"] = nil
_G.engine_update = nil
local ok2 = pcall(loadfile("scripts/title_demo_entry.lua"))
if ok2 and _G.engine_update then
    _G.engine_update(0.016)
    _G.engine_update(0.016)
end
check("exit quits engine", _G._mock_quit == true)

local failed = 0
for _, okv in ipairs(results or {}) do if not okv then failed = failed + 1 end end
package.preload["kag_runner"] = _preload_kr
package.preload["backend"] = _preload_backend
package.loaded["backend"] = _loaded_backend
-- The mock left package.loaded["kag_runner"] = 3-key stub (start/update/
-- on_click). Reload the REAL module so later suite tests (and the
-- replay/debug features) see the full API -- native require still works
-- here (before test_sandbox locks require to package.loaded).
package.loaded["kag_runner"] = nil
local okr, kr = pcall(require, "kag_runner")
if not okr then
    print("[test_title_entry] kag_runner reload failed: " .. tostring(kr))
end

if failed > 0 then os.exit(1) end
print("TITLE ENTRY TESTS DONE")
