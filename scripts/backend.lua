-- ===========================================================================
--  Caesura (AmeKAG) -- backend.lua
--  Spec [0.4]: Unified C++ backend proxy.
--  Resolution order: 1. _CAESURA_BACKEND  2. direct KAG/Render/DevCore
-- ===========================================================================

local Backend = {}

local function get_backend()
    return rawget(_G, "_CAESURA_BACKEND")
end

-- =========================================================================
-- Audio
-- =========================================================================

function Backend.audio_play(bus, file, opts)
    opts = opts or {}
    local b = get_backend()
    if b then
        if bus == "bgm" then return b.audio("play_bgm", file, tonumber(opts.fadein) or 1.0)
        elseif bus == "voice" then return b.audio("play_voice", file)
        elseif bus == "se" then
            if opts.x and opts.y then return b.audio("play_se_3d", file, opts.x, opts.y, opts.z or 0)
            else return b.audio("play_se", file) end
        end
    else
        if bus == "bgm" then return KAG.play_bgm(file, tonumber(opts.fadein) or 1.0)
        elseif bus == "voice" then return KAG.play_voice(file)
        elseif bus == "se" then
            if opts.x and opts.y then return KAG.play_se_3d(file, opts.x, opts.y, opts.z or 0)
            else return KAG.play_se(file) end
        end
    end
    return false
end

function Backend.audio_stop(bus, opts)
    opts = opts or {}
    local b = get_backend()
    if b then
        if bus == "bgm" then return b.audio("stop_bgm", tonumber(opts.fadeout) or 1.0)
        elseif bus == "voice" then return b.audio("stop_voice")
        elseif bus == "se" then return b.audio("stop_se") end
    else
        if bus == "bgm" then return KAG.stop_bgm(tonumber(opts.fadeout) or 1.0)
        elseif bus == "voice" then return KAG.stop_voice()
        elseif bus == "se" then KAG.stop_se() end
    end
    return false
end

function Backend.audio_is_playing(bus)
    local b = get_backend()
    if b then
        if bus == "voice" then return b.audio("is_voice_playing") end
        if bus == "bgm"   then return b.audio("is_bgm_playing") end
        if bus == "se"    then return b.audio("is_playing", "se") end
    else
        if bus == "voice" then return KAG.is_voice_playing() end
        if bus == "bgm"   then return KAG.is_bgm_playing() end
        if bus == "se"    then return KAG.is_se_playing() end
    end
    return false
end

function Backend.audio_set_listener(px, py, pz, ax, ay, az)
    local b = get_backend()
    if b then return b.audio("set_listener", px or 0, py or 0, pz or 0, ax or 0, ay or 1, az or 0)
    else return KAG.set_listener(px, py, pz, ax, ay, az) end
end

function Backend.audio_set_bus_volume(bus, vol)
    local b = get_backend()
    if b then return b.audio("set_bus_volume", bus, vol)
    else return KAG.set_bus_volume(bus, vol) end
end

function Backend.audio_get_bus_volume(bus)
    local b = get_backend()
    if b then return b.audio("get_bus_volume", bus)
    else return KAG.get_bus_volume(bus) end
end

function Backend.audio_fade_volume(bus, target_vol, fade_time)
    local b = get_backend()
    if b then return b.audio("fade_volume", bus, target_vol, fade_time)
    else return false end
end

function Backend.audio_get_length(bus)
    local b = get_backend()
    if b then return b.audio("get_length", bus)
    else return 0 end
end

function Backend.audio_get_position(bus)
    local b = get_backend()
    if b then return b.audio("get_position", bus)
    else return 0 end
end

function Backend.stop_se()
    local b = get_backend()
    if b then return b.audio("stop_se")
    else return KAG.stop_se() end
end

function Backend.flush_wave_cache()
    local b = get_backend()
    if b then return b.audio("flush_wave_cache")
    else return KAG.flush_wave_cache() end
end

-- =========================================================================
-- Render
-- =========================================================================

function Backend.create_viewport(w, h)
    local b = get_backend()
    if b then return b.render("create_viewport", w, h)
    else return Render.create_viewport(w, h) end
end

function Backend.destroy_viewport(vpId)
    local b = get_backend()
    if b then return b.render("destroy_viewport", vpId)
    else return Render.destroy_viewport(vpId) end
end

function Backend.draw_viewport(vpId, x, y, w, h)
    local b = get_backend()
    if b then return b.render("draw_viewport", vpId, x, y, w, h)
    else return Render.draw_viewport(vpId, x, y, w, h) end
end

function Backend.fill_viewport(handleId, r, g, b, a)
    local bb = get_backend()
    if bb then return bb.render("fill_viewport", handleId, r, g, b, a)
    else return Render.fill_viewport(handleId, r, g, b, a) end
end

function Backend.load_texture(file)
    local b = get_backend()
    if b then return b.render("load_texture", file)
    else return Render.load_texture(file) end
end

function Backend.destroy_texture(id)
    local b = get_backend()
    if b then return b.render("destroy_texture", id)
    else return Render.destroy_texture(id) end
end

function Backend.create_solid_texture(r, g, b, a)
    local bb = get_backend()
    if bb then return bb.render("create_solid_texture", r, g, b, a)
    else return Render.create_solid_texture(r, g, b, a) end
end

function Backend.submit_batch(batch)
    local b = get_backend()
    if b then return b.render("submit_batch", batch)
    else return false end
end

function Backend.submit_blend(viewId, texA, texB, mode, progress, alpha)
    local b = get_backend()
    if b then return b.render("submit_blend", viewId, texA, texB, mode, progress, alpha)
    else return false end
end

function Backend.submit_transition(viewId, fromId, toId, ruleId, method, progress)
    local b = get_backend()
    if b then return b.render("submit_transition", viewId, fromId, toId, ruleId, method, progress)
    else return false end
end

function Backend.submit_vfx(viewId, texId, effect, fadeAlpha, fadeR, fadeG, fadeB, blurRadius, quakeX, quakeY)
    local b = get_backend()
    if b then return b.render("submit_vfx", viewId, texId, effect, fadeAlpha, fadeR, fadeG, fadeB, blurRadius, quakeX, quakeY)
    else return false end
end

function Backend.submit_stretch_blt(dst_rt, dst_rect, src_rt, src_rect, filter_id)
    local b = get_backend()
    if b then return b.render("submit_stretch_blt", dst_rt, dst_rect, src_rt, src_rect, filter_id)
    else return false end
end

function Backend.submit_affine_blt(dst_rt, dst_rect, src_rt, src_rect, matrix, filter_id)
    local b = get_backend()
    if b then return b.render("submit_affine_blt", dst_rt, dst_rect, src_rt, src_rect, matrix, filter_id)
    else return false end
end

-- =========================================================================
-- Text / UI
-- =========================================================================

function Backend.render_text(text, x, y, r, g, b, a)
    return KAG.show_text(text, x, y, r, g, b, a)
end

function Backend.show_text(text)
    return KAG.show_text(text)
end

function Backend.show_image(file, x, y)
    return KAG.show_image(file, x or 0, y or 0)
end

function Backend.clear_screen()
    return KAG.clear_screen()
end

function Backend.wait_click()
    return KAG.wait_click()
end

-- =========================================================================
-- System / Platform
-- =========================================================================

function Backend.set_resolution(w, h) return DevCore.set_resolution(w, h) end

function Backend.get_resolution()
    local b = get_backend()
    if b then return b.platform("get_resolution")
    else return DevCore.get_resolution() end
end

function Backend.set_input_focus(mode)
    local b = get_backend()
    if b then return b.platform("set_input_focus", mode)
    else return DevCore.set_input_focus(mode) end
end

function Backend.get_input_focus()
    local b = get_backend()
    if b then return b.platform("get_input_focus")
    else return DevCore.get_input_focus() end
end

function Backend.set_fullscreen(enabled)
    local b = get_backend()
    if b then return b.platform("set_fullscreen", enabled)
    else return DevCore.set_fullscreen(enabled) end
end


-- =========================================================================
-- Audio crossfade helper
-- =========================================================================

function Backend.audio_xfade(bus, new_file, fade_time)
    local b = get_backend()
    if b then return b.audio("xfade", bus, new_file, fade_time)
    else
        local Audio = require("audio")
        return Audio.crossfade_bgm(new_file, fade_time / 1000)
    end
end

-- =========================================================================
-- Texture management (name-based)
-- =========================================================================

function Backend.free_texture(name)
    local b = get_backend()
    if b then return b.render("free_texture", name)
    else return false end
end

-- =========================================================================
-- Text rendering helpers
-- =========================================================================

function Backend.clear_text()
    local b = get_backend()
    if b then return b.render("clear_text")
    else return KAG.clear_text() end
end

function Backend.render_ruby(text, ruby, x, y)
    local b = get_backend()
    if b then return b.render("render_ruby", text, ruby, x, y)
    else return KAG.render_ruby(text, ruby, x, y) end
end

function Backend.set_font(id)
    local b = get_backend()
    if b then return b.render("set_font", id)
    else return KAG.set_font(id) end
end

function Backend.line_height()
    local b = get_backend()
    if b then return b.render("line_height")
    else return 24 end
end

-- =========================================================================
-- Logging
-- =========================================================================

function Backend.log(msg)
    local b = get_backend()
    if b then return b.platform("log", msg)
    else print("[Caesura] " .. tostring(msg)) end
end


-- =========================================================================
-- Particle effects (stubs for pure Lua fallback)
-- =========================================================================

function Backend.begin_particles()
    local b = get_backend()
    if b then return b.render("begin_particles")
    else return true end
end

function Backend.draw_particle(x, y, size, ptype)
    local b = get_backend()
    if b then return b.render("draw_particle", x, y, size, ptype)
    else return true end
end

function Backend.end_particles()
    local b = get_backend()
    if b then return b.render("end_particles")
    else return true end
end

function Backend.clear_particles()
    local b = get_backend()
    if b then return b.render("clear_particles")
    else return true end
end


-- ══════════════════════════════════════════════════════
-- 异步加载 API (G11-U3)
-- ══════════════════════════════════════════════════════

--- 异步加载纹理 (立即返回 request_id; 可选 callback(success, path, texId))
function Backend.load_texture_async(path, callback)
    local b = get_backend()
    if b then return b.render("load_texture_async", path, callback) end
end

--- 取消所有异步加载
function Backend.cancel_async_loads()
    local b = get_backend()
    if b then return b.render("cancel_async_loads") end
end
return Backend

