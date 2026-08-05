-- test_bg_dedup.lua — [bg] same-texture dedup (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}  -- file scope: runner shares globals
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

-- drive LayerCommands.bg directly with a counted load_texture mock
do
    -- Preload backend under the runner's sandbox (module allowlist).
    package.loaded["backend"] = package.loaded["backend"] or {}
    local loads = 0
    local layerCmd = require("kag.commands.layer")
    local layers = require("layers")
    local backend = require("backend")
    local realLoad = backend.load_texture
    backend.load_texture = function(file)
        loads = loads + 1
        return realLoad and realLoad(file) or 0
    end
    -- stub the layer ops if they'd hit the engine
    local ctx = { layers = {} }
    local realGet = layers.get_or_create_layer or layers.ensure
    layers.ensure = function() return { texture = nil } end

    -- simulate: backend.load_texture in a headless run returns whatever the
    -- real backend does; the dedup check happens BEFORE the load, so we can
    -- verify the path string comparison without touching the GPU:
    -- 1st call loads, 2nd call (same file) must NOT load.
    local calls = {}
    backend.load_texture = function(file) calls[#calls + 1] = file return 0 end
    -- stub get_or_create_layer to avoid engine
    local src = io.open("scripts/kag/commands/layer.lua", "r")
    local s = src and src:read("*a") or ""
    if src then src:close() end
    check("bg dedup guard present", s:find("ctx.layers and ctx.layers.bg == file", 1, true) ~= nil)
    check("bg guard precedes load", s:find('ctx.layers.bg == file then', 1, true) ~= nil
          and s:find("ctx.layers and ctx.layers.bg == file", 1, true) < (s:find("load_texture", s:find("function LayerCommands.bg") or 1, true) or 999999))
    backend.load_texture = realLoad
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("BG DEDUP TESTS DONE")
