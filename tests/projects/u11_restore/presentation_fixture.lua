-- Test-host composition only. Every render/save/restore call below uses the
-- engine's registered native backend; consumer never builds the saved page.
package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
local config = require("config")
local runner = require("kag_runner")
local saves = require("kag.commands.save")
local layers = require("layers")
local text = require("kag.text_scene")
local M = {rendered_frames=0}

config.accessibility.color_filter = "none"
config.accessibility.cc_mode = false
assert(DevCore.set_resolution(640, 360))

function engine_update() end -- The host advances the real runner explicitly.
function engine_render()
    local ok, err = pcall(function()
        layers.render()
        if runner.get_ctx() then assert(runner.render()) end
    end)
    M.rendered_frames = M.rendered_frames + 1
    if ok then M.last_render_error = nil else M.last_render_error = tostring(err) end
    if not ok then error(err, 0) end
end

local function add_image(id, path, x, y, w, h, z, opacity)
    local node = layers.add_layer(nil, {
        id=id, name=id, x=x, y=y, w=w, h=h, z=z, opacity=opacity or 255,
    })
    layers.set_layer_image(node, assert(Render.load_texture(path)))
    return node
end

local function fresh()
    assert(runner.stop())
    assert(runner.start("tests/projects/u11_restore/base.ks"))
    for _=1,4 do runner.update(0) end
    local ctx = assert(runner.get_ctx())
    assert(ctx._executing_command == "wait" and ctx.f.route == "saved")
    ctx.cc_mode = false
    return ctx
end

function M.producer_page()
    local ctx = fresh()
    assert(layers.clear_for_restore())
    add_image("u11_background", "assets/u11/background.bmp", 0, 0, 640, 360, 0)
    add_image("u11_foreground", "assets/u11/foreground.bmp", 392, 42, 176, 148, 1, 177)
    local panel = layers.add_layer(nil, {
        id="u11_message", name="u11_message", x=20, y=220,
        w=600, h=122, z=2, opacity=231,
    })
    layers.set_layer_image(panel, assert(Render.create_solid_texture(8, 15, 30, 255)))
    assert(Render.text_set_font("assets/fonts/NotoSansCJKsc-Regular.otf", 24))
    text.reset(ctx)
    text.add_text(ctx, "U11  COLD RESTORE", 36, 247,
        {249, 218, 125, 255}, "title", 1, false, false, true)
    text.add_text(ctx, "恢复页面测试 ABCD", 36, 299,
        {230, 245, 255, 255}, "body", 1.1, true, false, false)
    text.add_ruby(ctx, "保存", "ほぞん", {
        x=442, y=294, start_x=442, max_width=160,
        color={158, 228, 195, 255}, group="ruby", font_size=24,
    })
    ctx.text_state.reveal_chars = 5
    ctx.text_state.opacity = 217
    ctx.text_speed = 40
    ctx.reveal = {total=11, elapsed=200, last_shown=5}
    ctx.f.secret = "u11-cold-presentation-must-be-encrypted"
    ctx.f.page_marker = "saved-page"
end

function M.changed_page()
    local ctx = assert(runner.get_ctx())
    assert(layers.clear_for_restore())
    add_image("u11_changed", "assets/u11/changed.bmp", 0, 0, 640, 360, 0)
    text.reset(ctx)
    text.add_text(ctx, "CHANGED PAGE - MUST NOT SURVIVE LOAD", 28, 155,
        {255, 245, 245, 255}, "changed", 1, false, false, true)
    ctx.f.page_marker = "future-page"
end

function M.bootstrap()
    local ctx = fresh()
    M.changed_page()
    ctx.f.bootstrap_only = true
end

function M.hide_text()
    assert(runner.get_ctx()).text_state.opacity = 0
end

function M.save()
    local ctx = assert(runner.get_ctx())
    saves.save(ctx, {slot=38, thumbnail="fixture-only"})
    assert(ctx.tf.save_result == "ok", ctx.tf.save_error)
end

function M.load()
    local previous = assert(runner.get_ctx())
    local ok, err = saves.load(previous, {slot=38})
    assert(ok, err)
    local ctx = assert(runner.get_ctx())
    assert(ctx ~= previous and ctx.f.page_marker == "saved-page")
    assert(ctx.f.bootstrap_only == nil)
    assert(ctx.text_state.reveal_chars == 5 and ctx.text_state.opacity == 217)
    local font = assert(Restore.capture_font())
    assert(font.active and font.font == 2 and font.size == 24)
end

return M
