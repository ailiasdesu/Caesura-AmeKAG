-- ===========================================================================
--  Caesura (AmeKAG) -- backend.lua
--  Spec [0.4]: Unified C++ backend proxy.
--  Resolution order: 1. _CAESURA_BACKEND  2. direct KAG/Render/DevCore
-- ===========================================================================

local Backend = {}

local function get_backend()
    return rawget(_G, "_CAESURA_BACKEND")
end

-- Resolution chain for backend convenience calls (audit: backend.lua
-- called a bare global KAG.* 18 times -- the ENGINE sets _G.KAG (a C++
-- binding table) so it worked there, but direct-API contexts had nil).
-- Order: _CAESURA_BACKEND (UnifiedBinding) -> global KAG -> kag module.
local function resolve(fn_name)
    local be = get_backend()
    local v = be and be[fn_name]
    if type(v) == "function" then return v end
    local g = rawget(_G, "KAG")
    v = g and g[fn_name]
    if type(v) == "function" then return v end
    local k = package.loaded["kag"]
    v = k and k[fn_name]
    if type(v) == "function" then return v end
    return nil
end

-- Guarded call (review should-fix): a missing binding must return nil
-- (direct-API contexts) instead of throwing "attempt to call a nil".
local function call_resolved(name, ...)
    local fn = resolve(name)
    if not fn then
        print("[backend] '" .. name .. "' unavailable (no KAG binding)")
        return nil
    end
    return fn(...)
end

-- =========================================================================
-- Audio
-- =========================================================================

-- Screen-offset pan (camera/quakes): shifts the main view rect.
function Backend.set_screen_offset(dx, dy)
    local be = get_backend()
    if be then return be.render("set_screen_offset", dx, dy)
    else return Render.set_screen_offset(dx, dy) end
end

function Backend.audio_play(bus, file, opts)
    opts = opts or {}
    local be = get_backend()
    if be then
        if bus == "bgm" then return be.audio("play_bgm", file, tonumber(opts.fadein) or 1.0)
        elseif bus == "voice" then return be.audio("play_voice", file)
        elseif bus == "se" then
            if opts.x and opts.y then return be.audio("play_se_3d", file, opts.x, opts.y, opts.z or 0)
            else return be.audio("play_se", file) end
        end
    else
        if bus == "bgm" then return call_resolved("play_bgm", file, tonumber(opts.fadein) or 1.0)
        elseif bus == "voice" then return call_resolved("play_voice", file)
        elseif bus == "se" then
            if opts.x and opts.y then return call_resolved("play_se_3d", file, opts.x, opts.y, opts.z or 0)
            else return call_resolved("play_se", file) end
        end
    end
    return false
end

function Backend.audio_stop(bus, opts)
    opts = opts or {}
    local be = get_backend()
    if be then
        if bus == "bgm" then return be.audio("stop_bgm", tonumber(opts.fadeout) or 1.0)
        elseif bus == "voice" then return be.audio("stop_voice")
        elseif bus == "se" then return be.audio("stop_se") end
    else
        if bus == "bgm" then return call_resolved("stop_bgm", tonumber(opts.fadeout) or 1.0)
        elseif bus == "voice" then return call_resolved("stop_voice")
        elseif bus == "se" then call_resolved("stop_se") end
    end
    return false
end

function Backend.audio_is_playing(bus)
    local be = get_backend()
    if be then
        if bus == "voice" then return be.audio("is_voice_playing") end
        if bus == "bgm"   then return be.audio("is_bgm_playing") end
        if bus == "se"    then return be.audio("is_playing", "se") end
    else
        if bus == "voice" then return call_resolved("is_voice_playing") end
        if bus == "bgm"   then return call_resolved("is_bgm_playing") end
        if bus == "se"    then return call_resolved("is_se_playing") end
    end
    return false
end

function Backend.audio_set_listener(px, py, pz, ax, ay, az)
    local be = get_backend()
    if be then return be.audio("set_listener", px or 0, py or 0, pz or 0, ax or 0, ay or 1, az or 0)
    else return call_resolved("set_listener", px, py, pz, ax, ay, az) end
end

function Backend.audio_set_bus_volume(bus, vol)
    local be = get_backend()
    if be then return be.audio("set_bus_volume", bus, vol)
    else return call_resolved("set_bus_volume", bus, vol) end
end

function Backend.audio_get_bus_volume(bus)
    local be = get_backend()
    if be then return be.audio("get_bus_volume", bus)
    else return call_resolved("get_bus_volume", bus) end
end

function Backend.audio_fade_volume(bus, target_vol, fade_time)
    local be = get_backend()
    if be then return be.audio("fade_volume", bus, target_vol, fade_time)
    else return false end
end

function Backend.audio_get_length(bus)
    local be = get_backend()
    if be then return be.audio("get_length", bus)
    else return 0 end
end

function Backend.audio_get_position(bus)
    local be = get_backend()
    if be then return be.audio("get_position", bus)
    else return 0 end
end

function Backend.stop_se()
    local be = get_backend()
    if be then return be.audio("stop_se")
    else return call_resolved("stop_se") end
end

function Backend.flush_wave_cache()
    local be = get_backend()
    if be then return be.audio("flush_wave_cache")
    else return call_resolved("flush_wave_cache") end
end

-- =========================================================================
-- Render
-- =========================================================================

function Backend.create_viewport(w, h)
    local be = get_backend()
    if be then return be.render("create_viewport", w, h)
    else return Render.create_viewport(w, h) end
end

function Backend.destroy_viewport(vpId)
    local be = get_backend()
    if be then return be.render("destroy_viewport", vpId)
    else return Render.destroy_viewport(vpId) end
end

function Backend.draw_viewport(vpId, x, y, w, h)
    local be = get_backend()
    if be then return be.render("draw_viewport", vpId, x, y, w, h)
    else return Render.draw_viewport(vpId, x, y, w, h) end
end

function Backend.fill_viewport(handleId, r, g, b, a)
    local bb = get_backend()
    if bb then return bb.render("fill_viewport", handleId, r, g, b, a)
    else return Render.fill_viewport(handleId, r, g, b, a) end
end

function Backend.load_texture(file)
    local be = get_backend()
    if be then return be.render("load_texture", file)
    else return Render.load_texture(file) end
end

function Backend.destroy_texture(id)
    local be = get_backend()
    if be then return be.render("destroy_texture", id)
    else return Render.destroy_texture(id) end
end

function Backend.create_solid_texture(r, g, b, a)
    local bb = get_backend()
    if bb then return bb.render("create_solid_texture", r, g, b, a)
    else return Render.create_solid_texture(r, g, b, a) end
end

function Backend.submit_batch(batch)
    local be = get_backend()
    if be then return be.render("submit_batch", batch)
    else return false end
end

function Backend.submit_blend(viewId, texA, texB, mode, progress, alpha)
    local be = get_backend()
    if be then return be.render("submit_blend", viewId, texA, texB, mode, progress, alpha)
    else return false end
end

function Backend.submit_transition(viewId, fromId, toId, ruleId, method, progress)
    local be = get_backend()
    if be then return be.render("submit_transition", viewId, fromId, toId, ruleId, method, progress)
    else return false end
end

function Backend.submit_vfx(texId, effect, fadeAlpha, fadeR, fadeG, fadeB, blurRadius, quakeX, quakeY)
    local be = get_backend()
    if be then return be.render("submit_vfx", texId, effect, fadeAlpha, fadeR, fadeG, fadeB, blurRadius, quakeX, quakeY)
    else return false end
end

function Backend.submit_stretch_blt(dst_rt, dst_rect, src_rt, src_rect, filter_id)
    local be = get_backend()
    if be then return be.render("submit_stretch_blt", dst_rt, dst_rect, src_rt, src_rect, filter_id)
    else return false end
end

function Backend.submit_affine_blt(dst_rt, dst_rect, src_rt, src_rect, matrix, filter_id)
    local be = get_backend()
    if be then return be.render("submit_affine_blt", dst_rt, dst_rect, src_rt, src_rect, matrix, filter_id)
    else return false end
end

-- =========================================================================
-- Text / UI
-- =========================================================================

function Backend.render_text(text, x, y, r, g, b, a)
    local be = get_backend()
    if be then return be.render("render_text", text, x, y, r, g, b, a)
    else return call_resolved("render_text", text, x, y, r, g, b, a) end
end

function Backend.show_text(text)
    return call_resolved("show_text", text)
end

function Backend.show_image(file, x, y)
    return call_resolved("show_image", file, x or 0, y or 0)
end

function Backend.clear_screen()
    return call_resolved("clear_screen")
end

function Backend.wait_click()
    -- Explicit require: the global KAG table is never set (review
    -- should-fix) -- the demo's direct-API path called into nil here.
    local kag_mod = require("kag")
    return kag_mod.wait_click()
end

-- =========================================================================
-- System / Platform
-- =========================================================================

function Backend.set_resolution(w, h)
    local be = get_backend()
    if be then return be.platform("set_resolution", w, h)
    else return DevCore.set_resolution(w, h) end
end

function Backend.get_resolution()
    local be = get_backend()
    if be then return be.platform("get_resolution")
    else return DevCore.get_resolution() end
end

function Backend.set_input_focus(mode)
    local be = get_backend()
    if be then return be.platform("set_input_focus", mode)
    else return DevCore.set_input_focus(mode) end
end

function Backend.get_input_focus()
    local be = get_backend()
    if be then return be.platform("get_input_focus")
    else return DevCore.get_input_focus() end
end

function Backend.set_fullscreen(enabled)
    local be = get_backend()
    if be then return be.platform("set_fullscreen", enabled)
    else return DevCore.set_fullscreen(enabled) end
end


-- =========================================================================
-- Audio crossfade helper
-- =========================================================================

function Backend.audio_xfade(bus, new_file, fade_time)
    local be = get_backend()
    if be then return be.audio("xfade", bus, new_file, fade_time)
    else
        local Audio = require("audio")
        return Audio.crossfade_bgm(new_file, fade_time / 1000)
    end
end

-- =========================================================================
-- Texture management (name-based)
-- =========================================================================



-- =========================================================================
-- Text rendering helpers
-- =========================================================================

function Backend.clear_text()
    local be = get_backend()
    if be then return be.render("clear_text")
    else return call_resolved("clear_text") end
end

function Backend.render_ruby(text, ruby, x, y, r, g, b, a)
    local be = get_backend()
    if be then
        return be.render("render_ruby", text, ruby, x, y, r, g, b, a)
    else
        return call_resolved("render_ruby", text, ruby, x, y, r, g, b, a)
    end
end

function Backend.set_font(id)
    local be = get_backend()
    if be then return be.render("set_font", id)
    else return call_resolved("set_font", id) end
end

function Backend.line_height()
    local be = get_backend()
    if be then return be.render("line_height")
    else return 24 end
end

-- =========================================================================
-- Logging
-- =========================================================================

function Backend.log(msg)
    local be = get_backend()
    if be then return be.platform("log", msg)
    else print("[Caesura] " .. tostring(msg)) end
end


-- =========================================================================
-- Particle effects (stubs for pure Lua fallback)
-- =========================================================================

-- clear_particles maps to the VFX binding (the render factory has no
-- "clear_particles" route; the dead begin/draw/end wrappers were removed).
function Backend.clear_particles()
    if VFX and VFX.particles_clear then
        return VFX.particles_clear()
    end
    return false
end

-- The emitter trio was missing -- [vfx type="particle" action="create"]
-- (and the new [particles] command) called backend.particles_create_emitter
-- and CRASHED with 'attempt to call a nil value' (audit: pre-existing).
function Backend.particles_create_emitter(cfg)
    if VFX and VFX.particles_create_emitter then
        return VFX.particles_create_emitter(cfg)
    end
    return false
end

function Backend.particles_emit(emitter, count)
    if VFX and VFX.particles_emit then
        return VFX.particles_emit(emitter, count)
    end
    return false
end

function Backend.particles_destroy_emitter(emitter)
    if VFX and VFX.particles_destroy_emitter then
        return VFX.particles_destroy_emitter(emitter)
    end
    return false
end


-- ══════════════════════════════════════════════════════
-- 异步加载 API (G11-U3)
-- ══════════════════════════════════════════════════════

--- 异步加载纹理 (立即返回 request_id; 可选 callback(success, path, texId))
function Backend.load_texture_async(path, callback)
    local be = get_backend()
    if be then return be.render("load_texture_async", path, callback) end
end

--- 取消所有异步加载
function Backend.cancel_async_loads()
    local be = get_backend()
    if be then return be.render("cancel_async_loads") end
end

-- =============================================================================
-- Text rendering state
-- =============================================================================

function Backend.text_set_font(face, size, color)
    Render.text_set_font(face, size, color)
end

function Backend.text_reset_state()
    Render.text_reset_state()
end
return Backend
