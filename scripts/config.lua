-- ═══════════════════════════════════════════════════════════════════════════
--  Caesura (AmeKAG) — config.lua
--  Engine configuration and backend selection.
--  Spec [0.4]: Uses BackendFactory to create unified backend proxy.
--  Spec [4.3]: Window settings + volume persistence via settings/ files.
--  Loaded BEFORE kag/init.lua and game_logic.lua.
-- ═══════════════════════════════════════════════════════════════════════════

config = {}
config.dev_mode = true  -- true = dev mode (lax sandbox), false = Release (strict sandbox)
config.entry_script = "demo.lua"  -- Change to "game_logic.lua" for automated tests  -- First script loaded after init ([10.2.30])

-- ═══════════════════════════════════════════════════════════════════
-- Backend Selection (spec [0.4] factory pattern)
-- ═══════════════════════════════════════════════════════════════════
--
-- Render backends: "bgfx" (default)
-- Audio backends:  "soloud" (default)
-- Platform:        "sdl3" (default)
--
-- The C++ side already created concrete backends during Engine::init().
-- BackendFactory reads config and constructs the Lua-side proxy.

config.render_backend   = "bgfx"
config.audio_backend    = "soloud"
config.platform_backend = "sdl3"

-- ═══════════════════════════════════════════════════════════════════
-- Window Settings (persisted across sessions)
-- ═══════════════════════════════════════════════════════════════════

config.window_width   = 1280
config.window_height  = 720
config.vsync          = true
config.texture_budget = "auto"  -- "auto" | 0|1|2|3|4|5: texture memory budget tier ([10.2.65])
config.adaptive_quality = true  -- Spec [10.2.40]: auto-detect GPU tier, scale effects/shadow/resolution
config.fullscreen     = false
config.window_title   = "Caesura"

-- Per-bus volume defaults (persisted across sessions)
config.bgm_volume   = 1.0
config.voice_volume = 1.0
config.se_volume    = 1.0

-- ═══════════════════════════════════════════════════════════════════
-- CARC validation ([10.2.30])
config.carc_verify_on_startup = true  -- Verify CARC signature before loading scripts

-- Persistence file paths (relative to game root)
-- ═══════════════════════════════════════════════════════════════════

config._VOLUME_FILE          = "settings/volume.lua"
config._WINDOW_SETTINGS_FILE = "settings/window.lua"
config._MAIN_CONFIG_FILE     = "settings/config.lua"

-- ── Path safety ──────────────────────────────────────────────────────────
-- All file I/O is confined to settings/ directory.

local SAFE_PREFIX = "settings/"

local function safe_path(path)
    local norm = path:gsub("\\", "/")
    if norm:find("^" .. SAFE_PREFIX:gsub("/", "/")) and not norm:find("%.%.") then
        return norm
    end
    return nil
end

local function ensure_dir(path)
    local dir = safe_path(path)
    if not dir then return false end
    local ok, _ = os.rename(dir, dir)
    if ok then return true end
    local parent = dir:match("^(.-)[^/]+/$")
    if parent then ensure_dir(parent) end
    local probe = io.open(dir .. ".dir_probe", "w")
    if probe then probe:close(); os.remove(dir .. ".dir_probe"); return true end
    return false
end

-- ═══════════════════════════════════════════════════════════════════
-- Volume Persistence — Spec [4.3]
-- ═══════════════════════════════════════════════════════════════════

-- Security: type whitelist for loaded config values
local VALID_TYPES = { number = true, string = true, boolean = true, ["nil"] = true }

local function validate_types(tbl, path)
    path = path or "config"
    for k, v in pairs(tbl) do
        local vt = type(v)
        if not VALID_TYPES[vt] then
            print(string.format("[config] Rejected key '%s.%s' with type '%s'", path, tostring(k), vt))
            return false
        end
    end
    return true
end
--- Load persisted bus volumes from settings/volume.lua
function config.load_volumes()
    local sp = safe_path(config._VOLUME_FILE)
    if not sp then return end
    local f, err = io.open(sp, "r")
    if not f then
        print("[config] No volume file, using defaults.")
        return
    end
    local code = f:read("*a")
    f:close()
    local chunk, loadErr = load(code, config._VOLUME_FILE, "t", {})
    if not chunk then
        print("[config] Volume file parse error: " .. tostring(loadErr))
        return
    end
    local ok, data = pcall(chunk)
    if ok and type(data) == "table" then
        if not validate_types(data) then return end
        config.bgm_volume   = tonumber(data.bgm_volume)   or config.bgm_volume
        config.voice_volume = tonumber(data.voice_volume) or config.voice_volume
        config.se_volume    = tonumber(data.se_volume)    or config.se_volume
        print(string.format("[config] Loaded volumes: bgm=%.2f voice=%.2f se=%.2f",
            config.bgm_volume, config.voice_volume, config.se_volume))
    end
end

--- Save current bus volumes to settings/volume.lua
function config.save_volumes()
    local Audio = require("audio")
    local vol_bgm   = Audio.get_bgm_volume()
    local vol_voice = Audio.get_voice_volume()
    local vol_se    = Audio.get_se_volume()

    config.bgm_volume   = vol_bgm
    config.voice_volume = vol_voice
    config.se_volume    = vol_se

    ensure_dir("settings/")
    local sp = safe_path(config._VOLUME_FILE)
    if not sp then return false end
    local f, err = io.open(sp, "w")
    if not f then
        print("[config] Cannot write volume file: " .. tostring(err))
        return false
    end
    f:write(string.format(
        "return { bgm_volume = %.2f, voice_volume = %.2f, se_volume = %.2f }\n",
        config.bgm_volume, config.voice_volume, config.se_volume
    ))
    f:close()
    print(string.format("[config] Saved volumes: bgm=%.2f voice=%.2f se=%.2f",
        config.bgm_volume, config.voice_volume, config.se_volume))
    return true
end

-- ═══════════════════════════════════════════════════════════════════
-- Window Settings Persistence — Spec [4.3]
-- ═══════════════════════════════════════════════════════════════════

--- Load persisted window settings from settings/window.lua
function config.load_window_settings()
    local sp = safe_path(config._WINDOW_SETTINGS_FILE)
    if not sp then return end
    local f, err = io.open(sp, "r")
    if not f then
        print("[config] No window settings file, using defaults.")
        return
    end
    local code = f:read("*a")
    f:close()
    local chunk, loadErr = load(code, config._WINDOW_SETTINGS_FILE, "t", {})
    if not chunk then
        print("[config] Window settings parse error: " .. tostring(loadErr))
        return
    end
    local ok, data = pcall(chunk)
    if ok and type(data) == "table" then
        config.window_width  = tonumber(data.window_width)  or config.window_width
        config.window_height = tonumber(data.window_height) or config.window_height
        config.fullscreen    = data.fullscreen
        config.vsync         = data.vsync
        config.window_title  = data.window_title or config.window_title
        print(string.format("[config] Loaded window: %dx%d fullscreen=%s vsync=%s",
            config.window_width, config.window_height,
            tostring(config.fullscreen), tostring(config.vsync)))
    end
end

--- Save current window settings to settings/window.lua
function config.save_window_settings()
    ensure_dir("settings/")
    local sp = safe_path(config._WINDOW_SETTINGS_FILE)
    if not sp then return false end
    local f, err = io.open(sp, "w")
    if not f then
        print("[config] Cannot write window settings: " .. tostring(err))
        return false
    end
    f:write(string.format(
        "return {\n" ..
        "  window_width  = %d,\n" ..
        "  window_height = %d,\n" ..
        "  fullscreen    = %s,\n" ..
        "  vsync         = %s,\n" ..
        "  thumbnail_quality = %d,\n" ..
        "  thumbnail_format  = %q,\n" ..
        "  window_title  = %q,\n" ..
        "}\n",
        config.window_width, config.window_height,
        tostring(config.fullscreen), tostring(config.vsync),
        config.thumbnail_quality,
        config.thumbnail_format,
        config.window_title or "Caesura"
    ))
    f:close()
    print(string.format("[config] Saved window: %dx%d fullscreen=%s",
        config.window_width, config.window_height, tostring(config.fullscreen)))
    return true
end

-- ═══════════════════════════════════════════════════════════════════
-- Unified Config — Spec [4.3] full config persistence
-- ═══════════════════════════════════════════════════════════════════

--- Load all persisted settings (volumes + window) from settings/config.lua
function config.load_all()
    local sp = safe_path(config._MAIN_CONFIG_FILE)
    if not sp then
        -- Fall back to individual files
        config.load_volumes()
        config.load_window_settings()
        return
    end
    local f, err = io.open(sp, "r")
    if not f then
        config.load_volumes()
        config.load_window_settings()
        return
    end
    local code = f:read("*a")
    f:close()
    local chunk, loadErr = load(code, config._MAIN_CONFIG_FILE, "t", {})
    if not chunk then
        print("[config] Main config parse error: " .. tostring(loadErr))
        config.load_volumes()
        config.load_window_settings()
        return
    end
    local ok, data = pcall(chunk)
    if ok and type(data) == "table" then
        if not validate_types(data) then return end
        config.bgm_volume    = tonumber(data.bgm_volume)    or config.bgm_volume
        config.voice_volume  = tonumber(data.voice_volume)  or config.voice_volume
        config.se_volume     = tonumber(data.se_volume)     or config.se_volume
        config.thumbnail_quality = tonumber(data.thumbnail_quality) or config.thumbnail_quality
        config.thumbnail_format  = data.thumbnail_format or config.thumbnail_format
        config.window_width  = tonumber(data.window_width)  or config.window_width
        config.window_height = tonumber(data.window_height) or config.window_height
        config.fullscreen    = data.fullscreen
        config.vsync         = data.vsync
        config.window_title  = data.window_title or config.window_title
        print(string.format("[config] Loaded unified config: %dx%d, bgm=%.2f",
            config.window_width, config.window_height, config.bgm_volume))
    end
end

--- Save all settings to settings/config.lua
function config.save_all()
    ensure_dir("settings/")
    local sp = safe_path(config._MAIN_CONFIG_FILE)
    if not sp then return false end
    local f, err = io.open(sp, "w")
    if not f then
        print("[config] Cannot write main config: " .. tostring(err))
        return false
    end
    f:write(string.format(
        "return {\n" ..
        "  bgm_volume    = %.2f,\n" ..
        "  voice_volume  = %.2f,\n" ..
        "  se_volume     = %.2f,\n" ..
        "  window_width  = %d,\n" ..
        "  window_height = %d,\n" ..
        "  fullscreen    = %s,\n" ..
        "  vsync         = %s,\n" ..
        "  thumbnail_quality = %d,\n" ..
        "  thumbnail_format  = %q,\n" ..
        "  window_title  = %q,\n" ..
        "}\n",
        config.bgm_volume, config.voice_volume, config.se_volume,
        config.window_width, config.window_height,
        tostring(config.fullscreen), tostring(config.vsync),
        config.thumbnail_quality,
        config.thumbnail_format,
        config.window_title or "Caesura"
    ))
    f:close()
    print("[config] Saved unified config.")
    return true
end

-- ═══════════════════════════════════════════════════════════════════
-- Apply: create unified backend via factory, store in registry
-- ═══════════════════════════════════════════════════════════════════

config._applied = false

local function apply()
    if config._applied then
        print("[config] Already applied, skipping.")
        return
    end
    local BackendFactory = require("backend_factory")

    local backend = BackendFactory.create({
        render   = config.render_backend,
        audio    = config.audio_backend,
        platform = config.platform_backend,
    })

    -- Validate
    local renderOk   = backend.render("ping")
    local audioOk    = backend.audio("ping")
    local platformOk = backend.platform("ping")

    if not (renderOk and audioOk and platformOk) then
        error("[config] Backend validation failed!")
    end

    -- Store validated backend as global singleton
    rawset(_G, "_CAESURA_BACKEND", backend)
    config._applied = true

    -- Load persisted settings from files, then apply
    config.load_all()
    backend.audio("set_bus_volume", "bgm",   config.bgm_volume)
    backend.audio("set_bus_volume", "voice", config.voice_volume)
    backend.audio("set_bus_volume", "se",    config.se_volume)

        -- Initialize particle system
    pcall(function()
        VFX.particles_init()
        print("[config] Particle system initialized.")
    end)

	print(string.format(
        "[config] Backends ready: render=%s audio=%s platform=%s",
        config.render_backend, config.audio_backend, config.platform_backend
    ))

    -- Apply persisted window settings (post-init resize)
    if DevCore and DevCore.set_resolution then
        local ok = DevCore.set_resolution(config.window_width, config.window_height)
        if ok then
            print(string.format("[config] Resolution applied: %dx%d", config.window_width, config.window_height))
        end
    end
    if config.fullscreen and DevCore and DevCore.set_fullscreen then
        DevCore.set_fullscreen(config.fullscreen)
    end
end

apply()
return config
