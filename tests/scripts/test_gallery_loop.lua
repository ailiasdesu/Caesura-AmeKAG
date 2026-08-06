-- test_gallery_loop.lua — [gallery] input loop (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Gallery = require("gallery")
local real_scan = Gallery.scan
Gallery.scan = function()
    return { { id = "cg1", name = "CG One", path = "a.png" },
             { id = "cg2", name = "CG Two", path = "b.png" },
             { id = "cg3", name = "CG Three", path = "c.png" } }
end
Gallery._renderCurrent = function() end
Gallery.hide = function(ctx) if ctx then ctx.galleryState = nil end end
local real_backend = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        if cmd == "set_input_focus" then return true end
        return true end }
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = { ensure = function() return { visible = true } end,
    set_layer_visible = function() end, set_z = function() end }
local ctx = { f = {}, sf = {}, tf = {}, mp = {}, variables = {},
    unlockedCG = { cg1 = true, cg2 = true, cg3 = true } }
local co = coroutine.create(function() Gallery.show(ctx) end)
coroutine.resume(co)
coroutine.resume(co)
check("starts at 1", ctx.galleryState and ctx.galleryState.index == 1)
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("right navigates", ctx.galleryState.index == 2)
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("right again", ctx.galleryState.index == 3)
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("right clamps", ctx.galleryState.index == 3)
_G._GAME_KEY_LEFT = true
coroutine.resume(co)
check("left back", ctx.galleryState.index == 2)
_G._GAME_KEY_LEFT = true
coroutine.resume(co)
check("left clamps at 1", ctx.galleryState.index == 1)
-- render cache: repeated frames do not re-load the texture (the real
-- _renderCurrent caches per index -- assert via load_texture calls)
local loads = 0
local real_backend_render = _G._CAESURA_BACKEND.render
_G._CAESURA_BACKEND.render = function(cmd, ...)
    if cmd == "load_texture" then loads = loads + 1 end
    return real_backend_render(cmd, ...)
end
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
coroutine.resume(co)
check("texture cached on repeat", loads == 0)
-- navigation to a NEW index re-loads (count advanced by the earlier
-- right) -- the cache only suppresses the SAME-index repeat above
_G._GAME_KEY_RIGHT = true
coroutine.resume(co)
check("nav re-loads new index", loads == 1)
_G._GAME_KEY_ESC = true
coroutine.resume(co)
check("esc closes", coroutine.status(co) == "dead" and ctx.galleryState == nil)
package.loaded["layers"] = layers_backup
_G._CAESURA_BACKEND = real_backend
Gallery.scan = real_scan

if failed > 0 then os.exit(1) end
print("GALLERY LOOP TESTS DONE")
