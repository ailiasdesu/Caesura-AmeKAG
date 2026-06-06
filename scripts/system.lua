-- ===========================================================================
--  Caesura (AmeKAG) — system.lua
--  System facilities: save/load, config, backlog, saveplace/loadplace.
--  Spec [4.1]: Backlog with voice + layers snapshots
--  Spec [4.2]: Save/Load with full ctx serialization
--  Spec [4.3]: Config with window + volume persistence
--  Krkrz reference: StorageIntf.h, TextStream.h, SystemIntf.h
-- ===========================================================================

local System = {}

-- Cache sandbox-vulnerable globals before lockdown (C3 + W8)
local _os_execute = os.execute
local _io_open = io.open
local _os_remove = os.remove
local _os_rename = os.rename

local _IS_WINDOWS = package.config:sub(1,1) == "\""

local saveDir = "saves/"

-- ===========================================================================
-- escape_string(str) -> Lua-safe string literal
-- ===========================================================================

local function escape_string(s)
    return (s:gsub("\\", "\\\\")
            :gsub("\"", "\\\"")
            :gsub("\n", "\\n")
            :gsub("\r", "\\r")
            :gsub("\t", "\\t"))
end

-- ===========================================================================
-- serialize_value(v, indent) -> string
-- Handles nil, boolean, number, string, table (recursive).
-- ===========================================================================

local function serialize_value(v, indent, visited)
    local t = type(v)
    if t == "nil" then
        return "nil"
    elseif t == "boolean" then
        return v and "true" or "false"
    elseif t == "number" then
        return tostring(v)
    elseif t == "string" then
        return '"' .. escape_string(v) .. '"'
    elseif t == "table" then
        return serialize_table(v, indent, visited)
    else
        return "nil --[[unsupported type: " .. t .. "]]"
    end
end

-- ===========================================================================
-- serialize_table(t, indent) -> string (Lua table constructor)
-- ===========================================================================

function serialize_table(t, indent, visited)
    if type(t) ~= "table" then
        return tostring(t)
    end
    visited = visited or {}
    if visited[t] then
        return '"<circular reference>"'
    end
    visited[t] = true
    indent = indent or ""
    local result = "{"
    local first = true
    local nextIndent = indent .. "  "

    -- Detect if array-like (consecutive integer keys starting at 1)
    local isArray = true
    local maxIdx = 0
    for k in pairs(t) do
        if type(k) ~= "number" or k < 1 or k ~= math.floor(k) then
            isArray = false
            break
        end
        if k > maxIdx then maxIdx = k end
    end
    if isArray and maxIdx == 0 then isArray = false end

    if isArray then
        for i = 1, maxIdx do
            local v = t[i]
            if v ~= nil then
                if not first then result = result .. "," end
                result = result .. "\n" .. nextIndent .. serialize_value(v, nextIndent, visited)
                first = false
            end
        end
    else
        -- Sort keys for deterministic output
        local keys = {}
        for k in pairs(t) do table.insert(keys, k) end
        table.sort(keys, function(a, b)
            if type(a) == type(b) then return tostring(a) < tostring(b) end
            return type(a) < type(b)
        end)

        for _, k in ipairs(keys) do
            local v = t[k]
            if not first then result = result .. "," end
            result = result .. "\n" .. nextIndent .. "[" .. serialize_value(k, nextIndent, visited) .. "] = " .. serialize_value(v, nextIndent, visited)
            first = false
        end
    end

    if not first then result = result .. "\n" .. indent end
    visited[t] = nil

    result = result .. "}"
    return result
end

-- ===========================================================================
-- Backlog management -- Spec [4.1]
-- ===========================================================================

--- System.push_backlog(ctx, text, voice_file) -- push entry to backlog
--- BacklogEntry: { text, voice, layers, timestamp, chapter, scene, token_index }
function System.push_backlog(ctx, text, voice_file)
    if not ctx then return end
    ctx.backlog = ctx.backlog or {}
    ctx.backlog_max = ctx.backlog_max or 500

    -- Capture layer snapshot if LayerTree available
    local layersSnapshot = nil
    pcall(function()
        local layers = require("layers")
        if layers and layers.capture_snapshot then
            layersSnapshot = layers.capture_snapshot(ctx.layers)
        end
    end)

    local entry = {
        text      = text or "",
        voice     = voice_file or "",
        layers    = layersSnapshot,
        timestamp = os.time(),
        chapter   = ctx.currentChapter or "",
        scene     = ctx.current_scene,
        token_index = ctx.token_index,
    }
    table.insert(ctx.backlog, entry)

    -- Trim if over max
    while #ctx.backlog > ctx.backlog_max do
        table.remove(ctx.backlog, 1)
    end
end

--- System.get_backlog_page(backlog, start_idx, count) -> BacklogEntry[]
function System.get_backlog_page(backlog, start_idx, count)
    backlog = backlog or {}
    count = count or 20
    start_idx = math.max(1, start_idx or 1)
    local result = {}
    for i = start_idx, math.min(start_idx + count - 1, #backlog) do
        table.insert(result, backlog[i])
    end
    return result
end

--- System.show_backlog(backlog) -- display backlog in UI
--- Spec [4.1] kag.history integration
function System.show_backlog(backlog)
    backlog = backlog or {}
    if #backlog == 0 then
        print("[System] Backlog is empty.")
        return
    end
    print("[System] === Backlog History ===")
    for i, entry in ipairs(backlog) do
        if type(entry) == "table" then
            local voice_info = (entry.voice and entry.voice ~= "") and (" [voice: " .. entry.voice .. "]") or ""
            print(string.format("  %d: %s%s", i, entry.text or "", voice_info))
        else
            print(string.format("  %d: %s", i, tostring(entry)))
        end
    end
    print("[System] === End Backlog ===")
end

--- System.replay_voice(index, ctx) -- replay voice from backlog entry
function System.replay_voice(index, ctx)
    if not ctx or not ctx.backlog then return false end
    local entry = ctx.backlog[index]
    if not entry then return false end

    -- Spec [4.1]: voice is stored as `voice` field
    local file = type(entry) == "table" and entry.voice or nil
    if not file or file == "" then return false end

    local Audio = require("audio")
    Audio.play_voice(file)
    return true
end

--- System.save_backlog(backlog, filepath) -- serialize backlog to file
function System.save_backlog(backlog, filepath)
    local serialized = System._serialize_backlog(backlog or {})
    local f, err = io.open(filepath, "w")
    if not f then
        print("[System] Cannot save backlog: " .. tostring(err))
        return false
    end
    f:write("return " .. serialize_table({entries = serialized}) .. "\n")
    f:close()
    return true
end

--- System.load_backlog(filepath) -- load backlog from file
function System.load_backlog(filepath)
    local f, err = io.open(filepath, "r")
    if not f then return {} end
    local code = f:read("*a")
    f:close()
    local chunk, loadErr = load(code, "backlog_load", "t", {})
    if not chunk then return {} end
    local ok, data = pcall(chunk)
    if ok and type(data) == "table" and data.entries then
        return data.entries
    end
    return {}
end

--- System.clear_backlog(ctx) -- clear all backlog entries
function System.clear_backlog(ctx)
    if ctx then ctx.backlog = {} end
end

-- Helper: serialize backlog entries preserving voice + layers fields
function System._serialize_backlog(backlog)
    local result = {}
    for i, entry in ipairs(backlog) do
        if type(entry) == "table" then
            result[i] = {
                text      = entry.text or "",
                voice     = entry.voice or "",
                timestamp = entry.timestamp or 0,
                chapter   = entry.chapter or "",
            }
        else
            result[i] = { text = tostring(entry), voice = "", timestamp = 0, chapter = "" }
        end
    end
    return result
end

-- ===========================================================================
-- Save / Load -- Spec [4.2]
-- SaveData: version, timestamp, scene, line_index, variables, sys_flags,
--           temp, layers, audio, config, backlog, call_stack, screenshot
-- ===========================================================================

--- System.capture_screenshot(ctx) -> binary PNG (C++ backend)
--- Spec [4.2]: uses bgfx requestScreenCap / readScreenCap
function System.capture_screenshot(ctx)
    local b = rawget(_G, "_CAESURA_BACKEND")
    if b then
        return b.render("capture_screenshot")
    end
    -- Fallback stub
    print("[System] Screenshot capture requires backend support.")
    return nil
end

--- System.get_save_info(slot) -> {timestamp, scene, screenshot}
--- Spec [4.2]: read save metadata without loading full state
function System.get_save_info(slot)
    local filename = saveDir .. "save_" .. slot .. ".lua"
    local f, err = _io_open(filename, "r")
    if not f then return nil end
    -- Read only header lines to avoid loading full save
    local header = ""
    for i = 1, 50 do
        local line = f:read("*l")
        if not line then break end
        header = header .. line .. "\n"
        if line:match("^%s*timestamp%s*=")
        or line:match("^%s*scene%s*=") then
            -- Continue reading relevant fields
        end
    end
    f:close()

    -- Parse full save for metadata
    local f2 = _io_open(filename, "r")
    if not f2 then return nil end
    local code = f2:read("*a")
    f2:close()

    local chunk, loadErr = load(code, "save_info", "t", {})
    if not chunk then return nil end
    local ok, data = pcall(chunk)
    if not ok or type(data) ~= "table" then return nil end

    return {
        timestamp  = data.timestamp or 0,
        scene      = data.scene or "",
        save_title = data.save_title or "",
        token_ptr  = data.token_ptr or 1,
    }
end

--- System.save(slot, ctx) -- save game state to slot file
function System.save(slot, ctx)
    -- Ensure save directory exists (cross-platform)
    if _IS_WINDOWS then
        _os_execute('if not exist "' .. saveDir:gsub("/$", "") .. '" mkdir "' .. saveDir:gsub("/$", "") .. '"')
    else
        _os_execute('mkdir -p "' .. saveDir .. '"')
    end

    -- Build comprehensive SaveData per Spec [4.2]
    local data = {
        version       = 2,
        engine_version = "1.0.0",
        slot          = slot,
        timestamp     = os.time(),
        scene         = ctx.currentScene or "",
        save_title    = ctx.save_title or "",
        token_ptr     = ctx.token_ptr or 1,

        -- KAG variables (f.xxx, sf.xxx, tf.xxx)
        variables     = ctx.f or {},
        sys_flags     = ctx.sf or {},
        temp          = ctx.tf or {},

        -- Layer snapshot
        layers        = nil,
        pcall(function()
            local layers = require("layers")
            if layers and layers.capture_snapshot and ctx.layers then
                data.layers = layers.capture_snapshot(ctx.layers)
            end
        end),

        -- Audio state
        audio         = {
            bgm_file   = ctx._bgmFile or "",
            bgm_pos    = ctx._bgmPos or 0,
            bgm_vol    = ctx._bgmVol or 1.0,
            voice_file = ctx._voiceFile or "",
        },

        -- Config snapshot
        config        = System._config or System.defaults,

        -- Backlog
        backlog       = System._serialize_backlog(ctx.backlog or {}),

        -- Text position tracking for save/load
        text_state    = ctx.text_state and table_deep_copy(ctx.text_state) or {},

        -- Call stack for [call]/[return]
        call_stack    = ctx.callStack or {},

        -- Waiting state
        waiting_input = ctx.waiting_input or false,
    }

    -- Capture screenshot via backend
    pcall(function()
        data.screenshot = System.capture_screenshot(ctx)
    end)

    local filename = saveDir .. "save_" .. slot .. ".lua"
    local ok = System._write_save_file(filename, data)
    print("[System] Saved to slot " .. slot)
    return ok
end

--- System._write_save_file(filename, data) -- internal serialization helper
function System._write_save_file(filename, data)
    local f, err = _io_open(filename, "w")
    if not f then
        print("[System] Cannot write save: " .. tostring(err))
        return false
    end

    -- Write header comment
    f:write("-- Caesura Save File v2\n")
    f:write("-- Slot: " .. (data.slot or 0) .. "\n")
    f:write("-- Timestamp: " .. (data.timestamp or 0) .. "\n")
    f:write("-- Scene: " .. (data.scene or "") .. "\n\n")

    -- Write serialized data
    f:write("return " .. serialize_table(data) .. "\n")
    f:close()
    return true
end

--- System.load(slot, ctx) -- load game state from slot file
function System.load(slot, ctx)
    local filename = saveDir .. "save_" .. slot .. ".lua"
    return System._load_from_file(filename, ctx)
end

--- System._load_from_file(filename, ctx) -- internal load helper
function System._load_from_file(filename, ctx)
    local f, err = _io_open(filename, "r")
    if not f then
        print("[System] Save not found: " .. tostring(err))
        return false
    end
    local code = f:read("*a")
    f:close()

    local chunk, loadErr = load(code, "save_load", "t", {})
    if not chunk then
        print("[System] Save parse error: " .. tostring(loadErr))
        return false
    end

    local ok, data = pcall(chunk)
    if not ok or type(data) ~= "table" then
        print("[System] Save data corrupt.")
        return false
    end


    -- Engine version check (Spec U6: archive version management)
    local loaded_version = data.engine_version or "0.0.0"
    if loaded_version ~= "1.0.0" then
        print(string.format("[System] Save version mismatch: %s (engine %s)", loaded_version, "1.0.0"))
        -- Future: call migrate function chain here
        -- For now: warn but continue loading
    end

    -- Restore KAG variables
    if data.variables then ctx.f = data.variables end
    if data.sys_flags  then ctx.sf = data.sys_flags end
    if data.temp       then ctx.tf = data.temp end

    -- Restore token pointer
    ctx.token_ptr = data.token_ptr or 1
    ctx.waiting_input = data.waiting_input or false
    ctx.currentScene  = data.scene or ctx.currentScene
    ctx.save_title    = data.save_title or ""

    -- Restore audio state
    if data.audio then
        ctx._bgmFile   = data.audio.bgm_file or ctx._bgmFile
        ctx._bgmVol    = data.audio.bgm_vol or ctx._bgmVol
        ctx._bgmPos    = data.audio.bgm_pos or ctx._bgmPos
        ctx._voiceFile = data.audio.voice_file or ctx._voiceFile

        -- Resume BGM if it was playing
        if data.audio.bgm_file and data.audio.bgm_file ~= "" then
            pcall(function()
                local Audio = require("audio")
                if Audio and Audio.resume_bgm then
                    Audio.resume_bgm(data.audio.bgm_file, data.audio.bgm_pos, data.audio.bgm_vol)
                end
            end)
        end
    end

    -- Restore layers
    if data.layers then
        pcall(function()
            local layers = require("layers")
            if layers and layers.restore_snapshot then
                layers.restore_snapshot(ctx.layers, data.layers)
            end
        end)
    end

    -- Restore config
    if data.config then
        System._config = data.config
        -- Apply persisted config to subsystems
        System._apply_config(ctx)
    end

    -- Restore backlog
    if data.backlog then
        ctx.backlog = {}
        for i, entry in ipairs(data.backlog) do
            table.insert(ctx.backlog, entry)
        end
    else
        ctx.backlog = {}
    end

    -- Restore call stack
    ctx.callStack = data.call_stack or {}

    -- Restore text position
    ctx.text_state = data.text_state or {}
    if data.text_state and data.text_state.line then
        pcall(function()
            local layers = require("layers")
            if layers and layers.restore_text_state then
                layers.restore_text_state(ctx.text_state)
            end
        end)
    end

    print("[System] Loaded from slot " .. (data.slot or 0))
    return true
end

-- ===========================================================================
-- ===========================================================================
-- table_deep_copy(t) -> table -- recursive deep copy (no metatables)
-- ===========================================================================
local function table_deep_copy(orig, copies)
    copies = copies or {}
    if type(orig) ~= "table" then return orig end
    if copies[orig] then return copies[orig] end
    local copy = {}
    copies[orig] = copy
    for k, v in next, orig do
        copy[table_deep_copy(k, copies)] = table_deep_copy(v, copies)
    end
    return copy
end

-- Saveplace / Loadplace -- in-memory scene bookmark
-- Spec [10.2.38]: independent temporary slot, no disk writes.
-- ===========================================================================

System._placeData = nil

--- System.saveplace(ctx) -- save scene bookmark (in-memory only)
function System.saveplace(ctx)
    System._placeData = {
        label = ctx.current_label,
        pc = ctx.pc,
        tf = ctx.tf and table_deep_copy(ctx.tf) or {},
        dialog_index = ctx.dialog_index,
        text_state = ctx.text_state and table_deep_copy(ctx.text_state) or {},
        layers = ctx.layers,  -- reference is fine
    }
    print("[System] Place saved (temporary bookmark).")
end

--- System.loadplace(ctx) -- restore scene bookmark from memory
function System.loadplace(ctx)
    if not System._placeData then
        print("[System] No place bookmark found.")
        return false
    end
    local pd = System._placeData
    ctx.current_label = pd.label
    ctx.pc = pd.pc
    ctx.tf = pd.tf
    ctx.dialog_index = pd.dialog_index
    ctx.text_state = pd.text_state or {}
    if pd.text_state and pd.text_state.line then
        pcall(function()
            local layers = require("layers")
            if layers and layers.restore_text_state then
                layers.restore_text_state(pd.text_state)
            end
        end)
    end
    print("[System] Place loaded.")
    return true
end

--- System.quick_save(ctx) -- alias for saveplace (backward compat)
function System.quick_save(ctx)
    System.saveplace(ctx)
end

--- System.quick_load(ctx) -- alias for loadplace (backward compat)
function System.quick_load(ctx)
    return System.loadplace(ctx)
end

--- System.clear_saves() -- delete all save files
function System.clear_saves()
    for slot = 0, 99 do
        local filename = saveDir .. "save_" .. slot .. ".lua"
        _os_remove(filename)
    end
    print("[System] All saves cleared.")
end

-- ===========================================================================
-- Config management -- Spec [4.3]
-- ===========================================================================

System.defaults = {
    -- Audio
    bgm_volume    = 1.0,
    voice_volume  = 1.0,
    se_volume     = 1.0,

    -- Text
    text_speed    = 50,
    auto_speed    = 3000,
    skip_mode     = "read",
    font_size     = 24,

    -- Display
    screen_width  = 1280,
    screen_height = 720,
    fullscreen    = false,
    window_title  = "Caesura",

    -- Feature toggles
    show_backlog  = true,
    enable_skip   = true,
    enable_auto   = true,
}

--- System.set_config(ctx, key, value) -- set a config item and notify subsystems
--- Spec [4.3]: Config:set propagates to AudioBus, SDL3, etc.
function System.set_config(ctx, key, value)
    System._config = System._config or {}
    System._config[key] = value

    -- Notify subsystems based on key
    if key == "bgm_volume" then
        pcall(function()
            local Audio = require("audio")
            if Audio and Audio.set_bus_volume then
                Audio.set_bus_volume("bgm", tonumber(value) or 1.0)
            end
        end)
        -- Also sync config.lua
        pcall(function()
            local config = require("config")
            if config then config.bgm_volume = tonumber(value) or 1.0 end
        end)
    elseif key == "voice_volume" then
        pcall(function()
            local Audio = require("audio")
            if Audio and Audio.set_bus_volume then
                Audio.set_bus_volume("voice", tonumber(value) or 1.0)
            end
        end)
        pcall(function()
            local config = require("config")
            if config then config.voice_volume = tonumber(value) or 1.0 end
        end)
    elseif key == "se_volume" then
        pcall(function()
            local Audio = require("audio")
            if Audio and Audio.set_bus_volume then
                Audio.set_bus_volume("se", tonumber(value) or 1.0)
            end
        end)
        pcall(function()
            local config = require("config")
            if config then config.se_volume = tonumber(value) or 1.0 end
        end)
    elseif key == "screen_width" or key == "screen_height" then
        local w = System._config.screen_width or 1280
        local h = System._config.screen_height or 720
        pcall(function()
            local backend = require("backend")
            if backend and backend.set_resolution then
                backend.set_resolution(w, h)
            end
        end)
        pcall(function()
            local config = require("config")
            if config then
                config.window_width = w
                config.window_height = h
            end
        end)
    elseif key == "fullscreen" then
        pcall(function()
            local backend = require("backend")
            if backend and backend.set_fullscreen then
                backend.set_fullscreen(value == true or value == "true")
            end
        end)
    end

    -- Auto-persist
    System.save_config(System._config, "settings/config.lua")
end

--- System.get_config(key) -> value
--- Spec [4.3]: Config:get with defaults fallback
function System.get_config(key)
    System._config = System._config or {}
    local val = System._config[key]
    if val ~= nil then return val end
    return System.defaults[key]
end

--- System._apply_config(ctx) -- apply all stored config to subsystems
function System._apply_config(ctx)
    local cfg = System._config or System.defaults

    -- Apply volumes
    pcall(function()
        local Audio = require("audio")
        if Audio and Audio.set_bus_volume then
            if cfg.bgm_volume   then Audio.set_bus_volume("bgm",   cfg.bgm_volume) end
            if cfg.voice_volume then Audio.set_bus_volume("voice", cfg.voice_volume) end
            if cfg.se_volume    then Audio.set_bus_volume("se",    cfg.se_volume) end
        end
    end)

    -- Apply display
    pcall(function()
        local backend = require("backend")
        if backend then
            if cfg.screen_width and cfg.screen_height and backend.set_resolution then
                backend.set_resolution(cfg.screen_width, cfg.screen_height)
            end
            if backend.set_fullscreen and cfg.fullscreen ~= nil then
                backend.set_fullscreen(cfg.fullscreen)
            end
        end
    end)
end

--- System.load_config(filepath) -> config table
function System.load_config(filepath)
    local f, err = io.open(filepath, "r")
    if not f then return nil end
    local code = f:read("*a")
    f:close()
    local chunk, loadErr = load(code, "config_load", "t", {})
    if not chunk then return nil end
    local ok, data = pcall(chunk)
    if ok and type(data) == "table" then
        System._config = data
        return data
    end
    return nil
end

--- System.save_config(config, filepath) -> boolean
function System.save_config(config, filepath)
    -- Ensure directory
    local dir = filepath:match("^(.-)[^/\\]+$") or ""
    if dir ~= "" and _IS_WINDOWS then
        _os_execute('if not exist "' .. dir:gsub("/$", ""):gsub("\\$", "") .. '" mkdir "' .. dir:gsub("/$", ""):gsub("\\$", "") .. '"')
    elseif dir ~= "" then
        _os_execute('mkdir -p "' .. dir .. '"')
    end

    local f, err = io.open(filepath, "w")
    if not f then
        print("[System] Cannot save config: " .. tostring(err))
        return false
    end
    f:write("return " .. serialize_table(config) .. "\n")
    f:close()
    return true
end

--- System.reset_config() -> defaults table
function System.reset_config()
    System._config = {}
    for k, v in pairs(System.defaults) do
        System._config[k] = v
    end
    System.save_config(System._config, "settings/config.lua")
    return System._config
end

-- ===========================================================================
-- Init: load persisted config on first require
-- ===========================================================================
System._config = System.load_config("settings/config.lua") or {}
if next(System._config) == nil then
    System._config = {}
    for k, v in pairs(System.defaults) do
        System._config[k] = v
    end
end


-- ===========================================================================
-- Phase G8-U1: Explicit GC hooks
-- ===========================================================================

--- System.collect_step(size) ? incremental GC step (size in KB)
function System.collect_step(size)
    collectgarbage("step", size or 10)
end

--- System.collect_full() ? full GC collect + audio cache cleanup
function System.collect_full()
    collectgarbage("collect")
    -- Clear audio cache if Audio module available
    pcall(function()
        local Audio = require("audio")
        if Audio and Audio.clear_cache then
            Audio.clear_cache()
        end
    end)
end

return System