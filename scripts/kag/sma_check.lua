-- =============================================================================
--  Caesura (AmeKAG) — kag/sma_check.lua
--  Static SMA asset checker (KAG Neo-Genesis tool, Battle 4d S3/S5).
--
--  Usage: lua scripts/kag/sma_check.lua <asset.json> [more.json ...]
--  Validates SMA JSON assets against the design-doc schema
--  (docs/design/skeletal-mesh-animation.md §2.2) AND the runtime
--  consumption contract of kag/sma.lua (weight layout, track/bone
--  references, frame times, indices multiple-of-3). Reports violations
--  with field paths. Exit code 0 = clean, 1 = violations (CI gate).
--
--  Library mode: require("kag.sma_check")
--    .validate(assetTable)         -> { ok, errors = {"path: msg", ...} }
--    .validate_file(path)          -> { ok, errors, meta = {...} }
--  Static-validation counterpart of the runtime loader: creators catch
--  bad assets before the game ever runs them.
-- =============================================================================

local BS = string.char(92)
-- sma_check lives in scripts/kag/ but modules are referenced as "kag.x"
-- from scripts/ (same convention as kag/*.lua): resolve the scripts/ dir.
local here = (arg and arg[0] and arg[0]:match("(.*[/" .. BS .. "])")) or "scripts/kag/"
local scripts_dir = here:gsub("kag[/\\]$", "")  -- scripts/ when here = scripts/kag/
package.path = scripts_dir .. "?.lua;" .. scripts_dir .. "kag/?.lua;" .. package.path

local sma_check = {}

-- JSON decode: prefer the shared json module (sma.lua does the same),
-- fall back to the driver's embedded parser when it is already loaded.
local json = nil
local json_ok, json_mod = pcall(require, "json")
if json_ok and type(json_mod) == "table" and json_mod.decode then
    json = json_mod
else
    local sma_ok, sma_mod = pcall(require, "kag.sma")
    if sma_ok and type(sma_mod) == "table" and sma_mod._json then
        json = sma_mod._json
    end
end

local function is_num(v) return type(v) == "number" end
local function is_int(v) return type(v) == "number" and v == math.floor(v) end
local function is_xy(v) return type(v) == "table" and #v == 2 and is_num(v[1]) and is_num(v[2]) end

local function push(errors, path, msg)
    errors[#errors + 1] = path .. ": " .. msg
end

-- ---------------------------------------------------------------------------
-- Mesh validation (shared by the top-level mesh and part variants).
-- weights layout: per-vertex 1 entry (single bone) or 2 interleaved
-- entries ([2i-1]/[2i], design doc §2.2). Weights are consumed verbatim
-- by the engine (no runtime normalization) -- creators must pre-normalize.
-- ---------------------------------------------------------------------------
local function check_mesh(mesh, path, errors, boneIds)
    if type(mesh) ~= "table" then
        push(errors, path, "required object (positions/uvs/indices/weights)")
        return
    end
    local pos = mesh.positions
    if type(pos) ~= "table" or #pos == 0 then
        push(errors, path .. ".positions", "required non-empty array of [x,y]")
        pos = {}
    end
    local n = #pos
    for i, p in ipairs(pos) do
        if not is_xy(p) then
            push(errors, path .. ".positions[" .. i .. "]", "must be an [x,y] number pair")
        end
    end
    local uvs = mesh.uvs
    if uvs ~= nil then
        if type(uvs) ~= "table" or #uvs ~= n then
            push(errors, path .. ".uvs", "must match positions length (" .. n .. ")")
        else
            for i, u in ipairs(uvs) do
                if not is_xy(u) then
                    push(errors, path .. ".uvs[" .. i .. "]", "must be an [u,v] number pair")
                end
            end
        end
    end
    local idx = mesh.indices
    if type(idx) ~= "table" or #idx == 0 then
        push(errors, path .. ".indices", "required non-empty array")
    else
        if #idx % 3 ~= 0 then
            push(errors, path .. ".indices", "length must be a multiple of 3 (triangles, got " .. #idx .. ")")
        end
        for i, ix in ipairs(idx) do
            if not is_int(ix) then
                push(errors, path .. ".indices[" .. i .. "]", "must be an integer vertex index")
            elseif ix < 0 or ix >= n then
                push(errors, path .. ".indices[" .. i .. "]",
                     "vertex index " .. tostring(ix) .. " out of range 0.." .. (n - 1))
            end
        end
    end
    local w = mesh.weights
    if w ~= nil then
        if type(w) ~= "table" or #w == 0 then
            push(errors, path .. ".weights", "must be a non-empty array (or omitted)")
        elseif #w ~= n and #w ~= n * 2 then
            push(errors, path .. ".weights",
                 "layout must be one entry per vertex (" .. n
                 .. ") or two interleaved entries (" .. (n * 2) .. "); got " .. #w)
        else
            for i, e in ipairs(w) do
                local wp = path .. ".weights[" .. i .. "]"
                if type(e) ~= "table" then
                    push(errors, wp, "must be an object {bone, w}")
                else
                    if not is_int(e.bone) or (boneIds and not boneIds[e.bone]) then
                        push(errors, wp .. ".bone",
                             "must reference a defined bone" .. (boneIds and " (id " .. tostring(e.bone) .. " undefined)" or ""))
                    end
                    if not is_num(e.w) or e.w < 0 or e.w > 1 then
                        push(errors, wp .. ".w", "must be a number in [0,1]")
                    end
                end
            end
            for v = 1, n do
                -- Interleaved layout matches the runtime (part_vertex_weights):
                -- vertex v -> entries [2v-1] (bone A) and [2v] (bone B).
                local e1 = w[(v - 1) * 2 + 1]
                local e2 = #w == n * 2 and w[(v - 1) * 2 + 2] or nil
                local ok1 = type(e1) == "table"
                local ok2 = type(e2) == "table"
                if ok1 and ok2 and e1.bone == e2.bone then
                    push(errors, path .. ".weights",
                         "vertex " .. (v - 1) .. " lists bone " .. tostring(e1.bone)
                         .. " twice; use two distinct bones or a single entry")
                end
                local sum = (ok1 and is_num(e1.w) and e1.w or 0) + (ok2 and is_num(e2.w) and e2.w or 0)
                if (ok1 or ok2) and math.abs(sum - 1) > 1e-3 then
                    push(errors, path .. ".weights",
                         "vertex " .. (v - 1) .. " weight sum " .. string.format("%.4f", sum)
                         .. " must be 1 (engine does not normalize)")
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------
-- validate(asset) -> {ok, errors}
-- ---------------------------------------------------------------------------
function sma_check.validate(asset)
    local errors = {}
    if type(asset) ~= "table" then
        return { ok = false, errors = { "root: expected a JSON object" } }
    end

    -- texture / atlas (optional metadata)
    if asset.texture ~= nil and type(asset.texture) ~= "string" then
        push(errors, "texture", "must be a string (or omitted)")
    end
    if asset.atlas ~= nil then
        if type(asset.atlas) ~= "table" or not is_int(asset.atlas.w) or not is_int(asset.atlas.h) then
            push(errors, "atlas", "must be {w, h} integers (or omitted)")
        elseif asset.atlas.w <= 0 or asset.atlas.h <= 0 then
            push(errors, "atlas", "w/h must be positive")
        end
    end

    -- bones: id uniqueness, parent references, pivot, acyclic parent chain
    local bones = asset.bones
    local boneIds = {}
    if type(bones) ~= "table" or #bones == 0 then
        push(errors, "bones", "required non-empty array of bone objects")
        bones = {}
    end
    for i, b in ipairs(bones) do
        local p = "bones[" .. i .. "]"
        if type(b) ~= "table" then
            push(errors, p, "must be an object {id, parent, pivot}")
        else
            if not is_int(b.id) then
                push(errors, p .. ".id", "required integer bone id")
            elseif boneIds[b.id] then
                push(errors, p .. ".id", "duplicate bone id " .. tostring(b.id))
            else
                boneIds[b.id] = true
            end
            if b.parent ~= nil and not is_int(b.parent) then
                push(errors, p .. ".parent", "must be an integer bone id or -1")
            end
            if not is_xy(b.pivot) then
                push(errors, p .. ".pivot", "required [x,y] number pair (pivot point)")
            end
        end
    end
    for i, b in ipairs(bones) do
        if type(b) == "table" and is_int(b.parent) and b.parent ~= -1 and not boneIds[b.parent] then
            push(errors, "bones[" .. i .. "].parent", "references undefined bone " .. tostring(b.parent))
        end
    end
    local byId = {}
    for i, b in ipairs(bones) do
        if type(b) == "table" and is_int(b.id) then byId[b.id] = b end
    end
    for i, b in ipairs(bones) do
        if type(b) == "table" and is_int(b.id) and is_int(b.parent) and b.parent ~= -1 then
            local seen, cur, steps = {}, b, 0
            while cur and cur.parent ~= nil and cur.parent ~= -1 do
                if seen[cur.id] or steps > #bones then
                    push(errors, "bones[" .. i .. "]", "parent chain contains a cycle")
                    break
                end
                seen[cur.id] = true
                cur = byId[cur.parent]
                steps = steps + 1
            end
        end
    end

    -- mesh (positions/uvs/indices/weights) — required UNLESS the asset
    -- is multi-part (spawn consumes parts.variants in that case).
    local parts = asset.parts
    local hasParts = type(parts) == "table" and #parts > 0
    if asset.mesh == nil and not hasParts then
        push(errors, "mesh", "required object (or provide non-empty parts)")
    elseif asset.mesh ~= nil then
        check_mesh(asset.mesh, "mesh", errors, boneIds)
    end

    -- animations: track/bone references, frame times ascending, duration
    local anims = asset.animations
    if type(anims) ~= "table" or next(anims) == nil then
        push(errors, "animations", "required non-empty object of clips")
    else
        for name, anim in pairs(anims) do
            local ap = "animations." .. tostring(name)
            if type(anim) ~= "table" then
                push(errors, ap, "must be an object {duration, tracks}")
            else
                if anim.duration ~= nil and (not is_num(anim.duration) or anim.duration <= 0) then
                    push(errors, ap .. ".duration", "must be a positive number (or omitted)")
                end
                local tracks = anim.tracks
                if tracks ~= nil and type(tracks) ~= "table" then
                    push(errors, ap .. ".tracks", "must be an array")
                    tracks = nil
                end
                local anyFrame = false
                for ti, tr in ipairs(tracks or {}) do
                    local tp = ap .. ".tracks[" .. ti .. "]"
                    if type(tr) ~= "table" then
                        push(errors, tp, "must be an object {bone, frames}")
                    else
                        if not is_int(tr.bone) or not boneIds[tr.bone] then
                            push(errors, tp .. ".bone",
                                 "must reference a defined bone (id " .. tostring(tr.bone) .. ")")
                        end
                        local frames = tr.frames
                        if type(frames) ~= "table" or #frames == 0 then
                            push(errors, tp .. ".frames", "required non-empty keyframe array")
                        else
                            anyFrame = true
                            local prevT = nil
                            for fi, f in ipairs(frames) do
                                local fp = tp .. ".frames[" .. fi .. "]"
                                if type(f) ~= "table" then
                                    push(errors, fp, "must be an object {t, rot?, scale?, offset?}")
                                else
                                    if not is_num(f.t) then
                                        push(errors, fp .. ".t", "required number timestamp")
                                    elseif prevT ~= nil and f.t < prevT then
                                        push(errors, fp .. ".t", "timestamps must be ascending ("
                                             .. tostring(prevT) .. " -> " .. tostring(f.t) .. ")")
                                    else
                                        prevT = f.t
                                    end
                                    if f.rot ~= nil and not is_num(f.rot) then
                                        push(errors, fp .. ".rot", "must be a number (radians)")
                                    end
                                    if f.scale ~= nil and not is_num(f.scale) then
                                        push(errors, fp .. ".scale", "must be a number")
                                    end
                                    if f.offset ~= nil and not is_xy(f.offset) then
                                        push(errors, fp .. ".offset", "must be an [x,y] number pair")
                                    end
                                end
                            end
                        end
                    end
                end
                if not anyFrame and anim.duration == nil then
                    push(errors, ap, "clip has no keyframes and no duration")
                end
            end
        end
    end

    -- parts: id uniqueness, variants as full mesh objects
    if parts ~= nil then
        if type(parts) ~= "table" or #parts == 0 then
            push(errors, "parts", "must be a non-empty array (or omitted)")
        else
            local partIds = {}
            for i, part in ipairs(parts) do
                local pp = "parts[" .. i .. "]"
                if type(part) ~= "table" then
                    push(errors, pp, "must be an object {id, variants}")
                else
                    local pid = part.id
                    if pid == nil then
                        push(errors, pp .. ".id", "required part id")
                    elseif partIds[pid] then
                        push(errors, pp .. ".id", "duplicate part id " .. tostring(pid))
                    else
                        partIds[pid] = true
                    end
                    if part.tex ~= nil and not is_num(part.tex) then
                        push(errors, pp .. ".tex", "must be a texture id number (or omitted)")
                    end
                    local variants = part.variants
                    if type(variants) ~= "table" or next(variants) == nil then
                        push(errors, pp .. ".variants", "required non-empty object of variant meshes")
                    else
                        for vname, vmesh in pairs(variants) do
                            check_mesh(vmesh, pp .. ".variants." .. tostring(vname), errors, boneIds)
                        end
                    end
                end
            end
        end
    end

    return { ok = #errors == 0, errors = errors }
end

-- ---------------------------------------------------------------------------
-- validate_file(path) -> {ok, errors, meta} — read + decode + validate,
-- plus a structure summary for tooling/editor panels.
-- ---------------------------------------------------------------------------
function sma_check.validate_file(path)
    local meta = { bones = 0, anims = {}, parts = 0, verts = 0, tris = 0 }
    if not json then
        return { ok = false, errors = { "file: no JSON decoder available" }, meta = meta }
    end
    local f = io.open(path, "r")
    if not f then
        return { ok = false, errors = { path .. ": cannot open file" }, meta = meta }
    end
    local text = f:read("*a")
    f:close()
    local ok, asset = pcall(json.decode, text)
    if not ok then
        return { ok = false, errors = { path .. ": malformed JSON (" .. tostring(asset) .. ")" }, meta = meta }
    end
    local res = sma_check.validate(asset)
    if type(asset) == "table" then
        meta.bones = type(asset.bones) == "table" and #asset.bones or 0
        -- Multi-part assets: report the first variant's mesh budget.
        local mesh = type(asset.mesh) == "table" and asset.mesh or nil
        if not mesh and type(asset.parts) == "table" then
            for _, part in ipairs(asset.parts) do
                if type(part) == "table" and type(part.variants) == "table" then
                    for _, v in pairs(part.variants) do
                        if type(v) == "table" then mesh = v break end
                    end
                end
                if mesh then break end
            end
        end
        meta.verts = mesh and type(mesh.positions) == "table" and #mesh.positions or 0
        meta.tris = mesh and type(mesh.indices) == "table" and math.floor(#mesh.indices / 3) or 0
        meta.parts = type(asset.parts) == "table" and #asset.parts or 0
        if type(asset.animations) == "table" then
            for name, anim in pairs(asset.animations) do
                local tc = type(anim) == "table" and type(anim.tracks) == "table" and #anim.tracks or 0
                meta.anims[#meta.anims + 1] = tostring(name) .. ":" .. tc
            end
            table.sort(meta.anims)
        end
    end
    return { ok = res.ok, errors = res.errors, meta = meta }
end

-- ---------------------------------------------------------------------------
-- CLI mode: validate each file, report, exit 1 on any violation.
-- Gate on the BASENAME of arg[0] (module loads keep their own arg[0] —
-- the main chunk's path — so a substring check would hijack requires
-- from e.g. test_sma_check.lua and os.exit() early).
-- ---------------------------------------------------------------------------
local cli_self = arg and arg[0] and (arg[0]:match("([^/" .. BS .. "]+)$") or "") == "sma_check.lua"
if cli_self then
    local total = 0
    for i = 1, #arg do
        local file = arg[i]
        local res = sma_check.validate_file(file)
        if res.ok then
            print("[sma_check] OK " .. file .. " (bones=" .. res.meta.bones
                  .. ", verts=" .. res.meta.verts .. ", tris=" .. res.meta.tris
                  .. ", anims=" .. #res.meta.anims .. ", parts=" .. res.meta.parts .. ")")
        else
            total = total + 1
            print("[sma_check] FAIL " .. file .. " (" .. #res.errors .. " error(s))")
            for _, e in ipairs(res.errors) do
                print("  " .. e)
            end
        end
    end
    os.exit(total == 0 and 0 or 1)
end

return sma_check
