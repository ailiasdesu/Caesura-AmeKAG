-- =============================================================================
--  Caesura (AmeKAG) — kag/sma.lua
--  Skeletal Mesh Animation driver (SMA, Battle 4d S3).
--
--  Loads SMA assets (docs/design/skeletal-mesh-animation.md §2.2 JSON),
--  resolves the bone hierarchy into world poses (parent chain + pivot),
--  LERPs animation tracks over time, and drives the engine IMeshRenderer
--  through the `sma.*` Lua binding (SmaBinding.cpp). The C++ side (SmaSkinner
--  + SmaMeshRenderer, S2) does the per-vertex weight blending + GPU draw.
--
--  Pure math (JSON parse / hierarchy / LERP) is unit-tested in
--  tests/scripts/test_sma.lua; GPU drawing follows the deferred pattern
--  (binding absent in tests -> every call is an inert no-op).
-- =============================================================================

local sma = {}

-- ---------------------------------------------------------------------------
-- Minimal JSON parser (SMA data is fixed-shape; a full parser is overkill).
-- Supports objects / arrays / strings / numbers / true / false / null.
-- ---------------------------------------------------------------------------
local json_ok, json = pcall(require, "json")
if not json_ok or type(json) ~= "table" or not json.decode then
    json = nil
end

if not json then
    local function json_parse(text)
        local pos, n = 1, #text
        local function skipws()
            while pos <= n do
                local c = text:sub(pos, pos)
                if c == " " or c == "\t" or c == "\n" or c == "\r" then
                    pos = pos + 1
                else break end
            end
        end
        local function parse_string()
            assert(text:sub(pos, pos) == '"', "expected string")
            pos = pos + 1
            local out = {}
            while pos <= n do
                local c = text:sub(pos, pos)
                if c == '"' then pos = pos + 1 return table.concat(out) end
                if c == "\\" then
                    pos = pos + 1
                    local e = text:sub(pos, pos)
                    if e == "n" then out[#out + 1] = "\n"
                    elseif e == "t" then out[#out + 1] = "\t"
                    elseif e == "r" then out[#out + 1] = "\r"
                    elseif e == "u" then
                        out[#out + 1] = utf8.char(tonumber(
                            text:sub(pos + 1, pos + 4), 16))
                        pos = pos + 4
                    else out[#out + 1] = e end
                    pos = pos + 1
                else
                    out[#out + 1] = c
                    pos = pos + 1
                end
            end
            error("unterminated string")
        end
        local parse_value
        parse_value = function()
            skipws()
            local c = text:sub(pos, pos)
            if c == "{" then
                pos = pos + 1
                local obj = {}
                skipws()
                if text:sub(pos, pos) == "}" then pos = pos + 1 return obj end
                while true do
                    skipws()
                    local key = parse_string()
                    skipws()
                    assert(text:sub(pos, pos) == ":", "expected :")
                    pos = pos + 1
                    obj[key] = parse_value()
                    skipws()
                    local sep = text:sub(pos, pos)
                    if sep == "," then pos = pos + 1
                    elseif sep == "}" then pos = pos + 1 return obj
                    else error("expected , or }") end
                end
            elseif c == "[" then
                pos = pos + 1
                local arr = {}
                skipws()
                if text:sub(pos, pos) == "]" then pos = pos + 1 return arr end
                while true do
                    arr[#arr + 1] = parse_value()
                    skipws()
                    local sep = text:sub(pos, pos)
                    if sep == "," then pos = pos + 1
                    elseif sep == "]" then pos = pos + 1 return arr
                    else error("expected , or ]") end
                end
            elseif c == '"' then
                return parse_string()
            elseif c == "t" then
                assert(text:sub(pos, pos + 3) == "true", "bad true")
                pos = pos + 4
                return true
            elseif c == "f" then
                assert(text:sub(pos, pos + 4) == "false", "bad false")
                pos = pos + 5
                return false
            elseif c == "n" then
                assert(text:sub(pos, pos + 3) == "null", "bad null")
                pos = pos + 4
                return nil
            else
                local num = text:match("^-?%d+%.?%d*[eE]?[+-]?%d*", pos)
                assert(num and #num > 0, "bad number at " .. pos)
                pos = pos + #num
                return tonumber(num)
            end
        end
        skipws()
        local result = parse_value()
        skipws()
        assert(pos > n, "trailing content")
        return result
    end
    json = { decode = json_parse }
end

sma._json = json

--- sma.load(jsonText) → parsed asset table (texture/atlas/bones/mesh/animations)
function sma.load(jsonText)
    return json.decode(jsonText)
end

-- ---------------------------------------------------------------------------
-- Bone hierarchy -> world poses (2x3 affine matrices), then extract into
-- the interface BonePose {rot, scale, ox, oy} (SmaSkinner applies that
-- around the origin; the pivot is baked into the offset here).
-- ---------------------------------------------------------------------------

local function local_matrix(pivot, rot, scale, ox, oy)
    local c, s = math.cos(rot), math.sin(rot)
    local a, d = scale * c, scale * c
    local b, cc = scale * s, -scale * s
    -- T(pivot + o) ∘ R·S ∘ T(-pivot)
    local px, py = pivot[1], pivot[2]
    return {
        a, b, cc, d,
        (px + ox) - (a * px + cc * py),
        (py + oy) - (b * px + d * py),
    }
end

local function compose(parent, local_m)
    -- parent ∘ local_m (apply local first, then parent)
    local pa, pb, pc, pd, ptx, pty = table.unpack(parent)
    local la, lb, lc, ld, ltx, lty = table.unpack(local_m)
    return {
        pa * la + pc * lb, pb * la + pd * lb,
        pa * lc + pc * ld, pb * lc + pd * ld,
        pa * ltx + pc * lty + ptx,
        pb * ltx + pd * lty + pty,
    }
end

local function to_bone_pose(m)
    local a, b = m[1], m[2]
    return {
        rot = math.atan(b, a), -- Lua 5.4: atan(y, x)
        scale = math.sqrt(a * a + b * b),
        ox = m[5],
        oy = m[6],
    }
end

local IDENTITY = { 1, 0, 0, 1, 0, 0 }

--- sma.resolve_world(bones, locals) → array of BonePose (world)
--  `locals[i]` = {rot, scale, ox, oy} in the PARENT frame. Bones must be
--  ordered parent-before-child (design contract); roots have parent -1.
function sma.resolve_world(bones, locals)
    local world = {}
    local matrices = {}
    for i, bone in ipairs(bones) do
        local l = locals[i] or { rot = 0, scale = 1, ox = 0, oy = 0 }
        local m = local_matrix(bone.pivot or { 0, 0 }, l.rot, l.scale,
            l.ox, l.oy)
        local parent = bone.parent or -1
        if parent >= 0 and matrices[parent + 1] then
            m = compose(matrices[parent + 1], m)
        end
        matrices[i] = m
        world[i] = to_bone_pose(m)
    end
    return world
end

-- ---------------------------------------------------------------------------
-- Animation tracks: LERP over keyframes.
-- ---------------------------------------------------------------------------

local function lerp(a, b, t) return a + (b - a) * t end

local function sample_track(frames, t)
    if #frames == 1 then
        local f = frames[1]
        return f.rot or 0, f.scale or 1,
            (f.offset and f.offset[1]) or 0, (f.offset and f.offset[2]) or 0
    end
    local prev = frames[1]
    for i = 2, #frames do
        local next_f = frames[i]
        if t <= next_f.t then
            local span = next_f.t - prev.t
            local k = (span > 0) and ((t - prev.t) / span) or 1
            k = math.max(0, math.min(1, k))
            return lerp(prev.rot or 0, next_f.rot or 0, k),
                lerp(prev.scale or 1, next_f.scale or 1, k),
                lerp((prev.offset and prev.offset[1]) or 0,
                     (next_f.offset and next_f.offset[1]) or 0, k),
                lerp((prev.offset and prev.offset[2]) or 0,
                     (next_f.offset and next_f.offset[2]) or 0, k)
        end
        prev = next_f
    end
    return prev.rot or 0, prev.scale or 1,
        (prev.offset and prev.offset[1]) or 0, (prev.offset and prev.offset[2]) or 0
end

--- sma.sample(asset, animName, t) → array of local BonePose (parent frame)
function sma.sample(asset, animName, t)
    local anim = asset and asset.animations and asset.animations[animName]
    if not anim then return nil end
    local locals = {}
    local bones = asset.bones or {}
    for i, bone in ipairs(bones) do
        local track
        for _, tr in ipairs(anim.tracks or {}) do
            if tr.bone == bone.id then track = tr break end
        end
        if track then
            local rot, scale, ox, oy = sample_track(track.frames or {}, t)
            locals[i] = { rot = rot, scale = scale, ox = ox, oy = oy }
        end
    end
    return locals
end

-- ---------------------------------------------------------------------------
-- Actor management + engine binding drive
-- ---------------------------------------------------------------------------

-- SmaBinding global (nil in unit tests / degraded). Resolved lazily so
-- tests can inject a recording mock after the module is loaded.
local function binding()
    return _G.sma
end

local function actor_pose_list(world)
    local poses = {}
    for _, p in ipairs(world) do
        poses[#poses + 1] = { p.rot, p.scale, p.ox, p.oy }
    end
    return poses
end

--- sma.spawn(ctx, name, asset, animName, opts) → actor
--  opts: {x, y, scale, texId, opacity, view}. Creates the engine mesh and
--  registers the actor for per-frame update/render.
function sma.spawn(ctx, name, asset, animName, opts)
    ctx.sma_actors = ctx.sma_actors or {}
    local old = ctx.sma_actors[name]
    if old and binding() then binding().destroy_mesh(old.handle) end

    opts = opts or {}
    local mesh = asset.mesh or {}
    local verts, indices = mesh.positions or {}, mesh.indices or {}
    -- verts: {{x, y}, ...} with per-vertex weights array (mesh.weights)
    local luaVerts = {}
    local weights = mesh.weights or {}
    for i, p in ipairs(verts) do
        local w = weights[i] or { bone = 0, w = 1 }
        local w2 = weights[i + 1] or {}
        luaVerts[#luaVerts + 1] = {
            p[1] or 0, p[2] or 0,
            (mesh.uvs and mesh.uvs[i] and mesh.uvs[i][1]) or 0,
            (mesh.uvs and mesh.uvs[i] and mesh.uvs[i][2]) or 0,
            w.bone or 0, w.w or 1,
            w2.bone or 0, w2.w or 0,
        }
    end
    local handle = 0
    if binding() then
        handle = binding().create_mesh(luaVerts, indices) or 0
    end
    local actor = {
        name = name,
        asset = asset,
        anim = animName,
        handle = handle,
        texId = opts.texId or 0,
        x = opts.x or 0,
        y = opts.y or 0,
        scale = opts.scale or 1,
        opacity = opts.opacity or 1,
        view = opts.view or 0,
        t = 0,
    }
    ctx.sma_actors[name] = actor
    return actor
end

--- sma.despawn(ctx, name)
function sma.despawn(ctx, name)
    local actors = ctx.sma_actors
    if not actors or not actors[name] then return end
    if binding() then binding().destroy_mesh(actors[name].handle) end
    actors[name] = nil
end

--- sma.update(ctx, dt) — advance animation time + re-skin every actor.
--  Call from the entry script's engine_update (KAG mode).
function sma.update(ctx, dt)
    local actors = ctx.sma_actors
    if not actors then return end
    for _, actor in pairs(actors) do
        actor.t = actor.t + dt
        local locals = sma.sample(actor.asset, actor.anim, actor.t)
        if locals then
            local world = sma.resolve_world(actor.asset.bones or {}, locals)
            if binding() and actor.handle and actor.handle ~= 0 then
                binding().update_mesh(actor.handle, actor_pose_list(world))
            end
        end
    end
end

--- sma.render(ctx) — draw every actor. Call from engine_render (KAG mode).
function sma.render(ctx)
    local actors = ctx.sma_actors
    if not actors then return end
    for _, actor in pairs(actors) do
        if binding() and actor.handle and actor.handle ~= 0 then
            binding().draw_mesh(actor.handle, actor.view, actor.texId,
                actor.x, actor.y, actor.scale, actor.opacity)
        end
    end
end

-- ---------------------------------------------------------------------------
-- KAG commands: [sma_play] / [sma_stop]
-- ---------------------------------------------------------------------------

--- [sma_play name="hero" asset="hero" anim="idle" x= y= scale= tex= view=]
--  asset must be registered via sma.register() (Lua/[iscript] side).
local assets = {}

--- sma.register(name, assetTable) — register a parsed SMA asset
function sma.register(name, assetTable)
    assets[name] = assetTable
end

--- sma.get(name) → registered asset or nil
function sma.get(name)
    return assets[name]
end

local sma_commands = {}

function sma_commands.sma_play(ctx, params)
    local name = params.name or params[1] or "actor"
    local asset = assets[params.asset or name]
    if not asset then
        print("[sma_play] unknown asset '" .. tostring(params.asset or name) .. "'")
        return
    end
    local anim = params.anim or "idle"
    sma.spawn(ctx, name, asset, anim, {
        x = tonumber(params.x) or 0,
        y = tonumber(params.y) or 0,
        scale = tonumber(params.scale) or 1,
        texId = tonumber(params.tex) or 0,
        opacity = tonumber(params.opacity) or 1,
        view = tonumber(params.view) or 0,
    })
end

function sma_commands.sma_stop(ctx, params)
    sma.despawn(ctx, params.name or params[1] or "actor")
end

sma.commands = sma_commands

return sma
