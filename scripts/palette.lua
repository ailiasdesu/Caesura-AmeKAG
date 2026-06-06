-- Caesura (AmeKAG) - Palette / LUT Management (Lua layer)
-- Provides load/apply/clear for 3D color lookup tables (LUTs).
-- All GPU operations go through the backend table (no direct bgfx calls).

local palette = {}

-- Internal registry: lut_id -> { handle, path, size }
local luts = {}

--- Load a LUT texture from file and register with a string ID.
-- @param lut_id  string identifier for this LUT (e.g. "vintage", "noir")
-- @param path    file path to the 256x16 or 4096x64 LUT image (.png)
-- @return true on success, nil+error on failure
function palette.load(lut_id, path)
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
    backend.set_palette(nil, 0.0, 0)
    return true
end

--- Unload a specific LUT from memory.
-- @param lut_id  string ID
-- @return true on success, nil+error if not found
function palette.unload(lut_id)
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

return palette