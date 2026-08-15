-- Caesura (AmeKAG) - Palette / LUT Management (Lua layer)
-- Provides load/apply/clear for 3D color lookup tables (LUTs).
-- All GPU operations go through the backend table (no direct bgfx calls).

local palette = {}

-- Internal registry: lut_id -> { handle, path, size }
local luts = {}

-- Round 72: the C++ side has NO LUT bindings yet (backend.set_palette and
-- backend.destroy_texture are not wired anywhere in src/ or the web bridge).
-- Guard every LUT-touching op so a [palette] tag degrades with a visible
-- notice instead of crashing mid-scene with a nil call.
local function lut_available()
    local b = backend  -- global; may be unset in headless probes
    return type(b) == "table"
        and type(b.set_palette) == "function"
        and type(b.destroy_texture) == "function"
end
local function lut_noop(what)
    print(string.format(
        "[palette] %s: LUT backend not wired (set_palette/destroy_texture missing) -- no-op",
        what))
end

--- Load a LUT texture from file and register with a string ID.
-- @param lut_id  string identifier for this LUT (e.g. "vintage", "noir")
-- @param path    file path to the 256x16 or 4096x64 LUT image (.png)
-- @return true on success, nil+error on failure
function palette.load(lut_id, path)
    -- Round 82: load() was the one LUT op without the lut_available() guard —
    -- set_night_mode() (and toggle_mode()) reached it headless and crashed on
    -- the nil backend global. Guard like apply/clear/unload: visible no-op
    -- instead of a mid-scene crash.
    if not lut_available() then lut_noop("load") return false end
    if not lut_id or #lut_id == 0 then
        return nil, "palette.load: lut_id required"
    end
    if not path or #path == 0 then
        return nil, "palette.load: path required"
    end

    -- Load texture through backend (TextureManager)
    local handle = backend.load_image(path)
    if not handle or not backend.is_valid(handle) then
        return nil, "palette.load: failed to load LUT image: " .. path
    end

    luts[lut_id] = {
        handle = handle,
        path   = path,
        size   = 16,  -- default 16^3; 4096x64 = 64^3
    }

    return true
end

--- Apply a loaded LUT to the current rendering output.
-- Intensity controls the blend: 0 = no effect, 1 = full LUT.
-- @param lut_id     string ID from palette.load()
-- @param intensity  float 0.0–1.0 (default 1.0)
-- @return true on success, nil+error on failure
function palette.apply(lut_id, intensity)
    if not lut_available() then lut_noop("apply") return false end
    intensity = intensity or 1.0
    intensity = math.max(0.0, math.min(1.0, intensity))

    local entry = luts[lut_id]
    if not entry then
        return nil, "palette.apply: LUT not loaded: " .. tostring(lut_id)
    end

    -- Request the backend to apply the palette for future submits
    -- The backend will bind s_lutTex and set u_paletteParams
    backend.set_palette(entry.handle, intensity, entry.size)

    return true
end

--- Clear/disable the active palette.
-- @return true
function palette.clear()
    if not lut_available() then lut_noop("clear") return end
    backend.set_palette(nil, 0.0, 0)
    return true
end

--- Unload a specific LUT from memory.
-- @param lut_id  string ID
-- @return true on success, nil+error if not found
function palette.unload(lut_id)
    if not lut_available() then lut_noop("unload") return end
    local entry = luts[lut_id]
    if not entry then
        return nil, "palette.unload: LUT not found: " .. tostring(lut_id)
    end

    if entry.handle and backend.is_valid(entry.handle) then
        backend.destroy_texture(entry.handle)
    end
    luts[lut_id] = nil
    return true
end

--- Unload all LUTs.
function palette.unload_all()
    for id, entry in pairs(luts) do
        if entry.handle and backend.is_valid(entry.handle) then
            backend.destroy_texture(entry.handle)
        end
    end
    luts = {}
end

--- Day/night mode state
palette._mode = "day"  -- "day" or "night"
palette._nightLutId = "__night_preset"

--- Activate day mode (neutral/no color grading).
-- Clears any active LUT. Safe to call repeatedly.
function palette.set_day_mode()
    palette._mode = "day"
    palette.clear()
    return true
end

--- Activate night mode (blue-dark color grading).
-- Tries to load assets/lut/night.png LUT. Falls back to clear if missing.
-- The LUT is cached after first load so subsequent calls are cheap.
function palette.set_night_mode()
    palette._mode = "night"

    -- Try loading the night LUT if not already loaded
    if not luts[palette._nightLutId] then
        local nightPath = "assets/lut/night.png"
        local ok = palette.load(palette._nightLutId, nightPath)
        if not ok then
            -- No night LUT file available; clear and exit gracefully
            palette.clear()
            return true, "night mode (no LUT file -- using neutral)"
        end
    end

    palette.apply(palette._nightLutId, 1.0)
    return true
end

--- Toggle between day and night mode.
-- @return "day" or "night"
function palette.toggle_mode()
    if palette._mode == "day" then
        palette.set_night_mode()
        return "night"
    else
        palette.set_day_mode()
        return "day"
    end
end

--- Get the current mode.
-- @return "day" or "night"
function palette.get_mode()
    return palette._mode
end

return palette