-- ===========================================================================
--  Caesura (AmeKAG) -- backend_factory.lua
--  Spec [0.4]: Factory pattern for backend creation.
--  Called once during engine init. Creates render/audio/platform backends,
--  stores handle to Lua registry, returns unified backend proxy.
-- ===========================================================================

local BackendFactory = {}

function BackendFactory.create(opts)
    opts = opts or {}

    local render_name   = opts.render   or "bgfx"
    local audio_name    = opts.audio    or "soloud"
    local platform_name = opts.platform or "sdl3"

    if Engine and Engine.select_platform_backend then
        Engine.select_platform_backend(platform_name)
    end

    if not KAG then error("[BackendFactory] KAG C binding not available.") end
    if not Render then error("[BackendFactory] Render C binding not available.") end
    if not DevCore then error("[BackendFactory] DevCore C binding not available.") end

    local backend = {}

    -- Render subsystem --------------------------------------------------
    backend.render = function(cmd, ...)
        if cmd == "ping" then return true
        elseif cmd == "name" then return render_name
        elseif cmd == "submit_batch" then return Render.submit_batch(...)
        elseif cmd == "submit_blend" then return Render.submit_blend(...)
        elseif cmd == "submit_transition" then return Render.submit_transition(...)
        elseif cmd == "submit_vfx" then return Render.submit_vfx(...)
        elseif cmd == "create_viewport" then return Render.create_viewport(...)
        elseif cmd == "destroy_viewport" then return Render.destroy_viewport(...)
        elseif cmd == "draw_viewport" then return Render.draw_viewport(...)
        elseif cmd == "fill_viewport" then return Render.fill_viewport(...)
        elseif cmd == "load_texture" then return Render.load_texture(...)
        elseif cmd == "destroy_texture" then return Render.destroy_texture(...)
        elseif cmd == "create_solid_texture" then
            return Render.create_solid_texture(...)
        elseif cmd == "get_resolution" then return Render.get_resolution()
        elseif cmd == "submit_stretch_blt" then return Render.submit_stretch_blt(...)
        elseif cmd == "submit_affine_blt" then return Render.submit_affine_blt(...)
        elseif cmd == "set_view_name" then return Render.set_view_name(...)
        else error("[BackendFactory] Unknown render: " .. tostring(cmd)) end
    end

    -- Audio subsystem ----------------------------------------------------
    backend.audio = function(cmd, ...)
        if cmd == "ping" then return true
        elseif cmd == "name" then return audio_name
        elseif cmd == "play_bgm" then return KAG.play_bgm(...)
        elseif cmd == "stop_bgm" then return KAG.stop_bgm(...)
        elseif cmd == "play_voice" then return KAG.play_voice(...)
        elseif cmd == "stop_voice" then return KAG.stop_voice(...)
        elseif cmd == "play_se" then return KAG.play_se(...)
        elseif cmd == "play_se_3d" then return KAG.play_se_3d(...)
        elseif cmd == "set_listener" then return KAG.set_listener(...)
        elseif cmd == "is_voice_playing" then return KAG.is_voice_playing()
        elseif cmd == "is_bgm_playing" then return KAG.is_bgm_playing()
        elseif cmd == "is_playing" then return KAG.is_se_playing()
        elseif cmd == "stop_se" then return KAG.stop_se()
        elseif cmd == "set_bus_volume" then return KAG.set_bus_volume(...)
        elseif cmd == "get_bus_volume" then return KAG.get_bus_volume(...)
        elseif cmd == "flush_wave_cache" then return KAG.flush_wave_cache()
        elseif cmd == "fade_volume" then return KAG.audio_fade_volume(...
        elseif cmd == "get_position" then return KAG.audio_get_position(...
        elseif cmd == "get_length" then return KAG.audio_get_length(...
        elseif cmd == "active_voice_count" then return 0
        else error("[BackendFactory] Unknown audio: " .. tostring(cmd)) end
    end

    -- Platform subsystem -------------------------------------------------
    backend.platform = function(cmd, ...)
        if cmd == "ping" then return true
        elseif cmd == "name" then return platform_name
        elseif cmd == "set_input_focus" then return DevCore.set_input_focus(...)
        elseif cmd == "get_input_focus" then return DevCore.get_input_focus()
        elseif cmd == "set_resolution" then return DevCore.set_resolution(...)
        elseif cmd == "get_resolution" then return DevCore.get_resolution()
        elseif cmd == "set_fullscreen" then return DevCore.set_fullscreen(...)
        elseif cmd == "log" then return DevCore.log(...)
        else error("[BackendFactory] Unknown platform: " .. tostring(cmd)) end
    end

    -- Text / UI shortcuts ------------------------------------------------
    backend.show_text = function(...) return KAG.show_text(...) end
    backend.show_image = function(...) return KAG.show_image(...) end
    backend.clear_screen = function(...) return KAG.clear_screen() end
    backend.wait_click = function(...) return KAG.wait_click(...) end

    rawset(_G, "_CAESURA_BACKEND", backend)
    print(string.format("[BackendFactory] Created: render=%s audio=%s platform=%s",
        render_name, audio_name, platform_name))
    return backend
end

return BackendFactory