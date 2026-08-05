-- =============================================================================
--  Caesura (AmeKAG) �� kag/commands/layer.lua
--  Phase 4: KAG layer tag handlers �� [bg], [fg], [cl], [image]
--  All calls route through layers.lua (spec [2.1]) + backend.lua.
-- =============================================================================

local backend = require("backend")
local layers  = require("layers")

local LayerCommands = {}

-- Internal: resolve file path (storage > path > file > positional)
local function resolve_file(params)
    return params.storage or params.path or params.file or params[1]
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Internal: resolve layer node by name, or create if missing
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local function get_or_create_layer(layerName, layerType)
    local node = layers.find(layerName)
    if not node then
        local root = layers.get_root()
        node = layers.add_layer(root, {
            name = layerName, tag = layerName,
            z = (layerType == layers.Type.LAYER_BASE and 0 or 1),
            x = 0, y = 0, w = 1280, h = 720, visible = true,
        })
    end
    return node
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [bg storage="bg/school.png"]
--  Set background layer (z=0) texture.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

-- Next-gen contracts: typed + clamped via kag/schema.
local schema = require("kag.schema")
schema.define("position", {
    x = { type = "number", default = 0 },
    y = { type = "number", default = 0 },
    scale = { type = "number", default = 1.0, min = 0.01, max = 16 },
})
schema.define("layopt", {
    opacity = { type = "number", default = 1.0, min = 0, max = 1.0 },
    visible = { type = "boolean", default = true },
})
schema.define("fadeout", {
    opacity = { type = "number", default = 0, min = 0, max = 1.0 },
    alpha = { type = "number", default = 0, min = 0, max = 1.0 },
    time = { type = "number", default = 500, min = 0, max = 30000 },
    duration = { type = "number", default = 500, min = 0, max = 30000 },
})

function LayerCommands.bg(ctx, params)
    local file = resolve_file(params)
    if not file then
        print("[LayerCmd] bg: no file specified")
        return
    end

    local tex = backend.load_texture(file)
    if not tex then
        print("[LayerCmd] bg: failed to load " .. file)
        return
    end

    local node = get_or_create_layer( "bg", layers.Type.LAYER_BASE)
    layers.set_layer_image(node, tex, nil, nil, nil, nil)
    layers.set_layer_visible(node, true)
    layers.set_z( node, 0)

    ctx.layers = ctx.layers or {}
    ctx.layers.bg = file
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [fg storage="chara/hero.png"]
--  Set foreground (character) layer (z=1) texture.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

function LayerCommands.fg(ctx, params)
    local file = resolve_file(params)
    if not file then
        print("[LayerCmd] fg: no file specified")
        return
    end

    local tex = backend.load_texture(file)
    if not tex then
        print("[LayerCmd] fg: failed to load " .. file)
        return
    end

    local node = get_or_create_layer( "fg", layers.Type.LAYER_LAYER0)
    layers.set_layer_image(node, tex, nil, nil, nil, nil)
    layers.set_layer_visible(node, true)
    layers.set_z( node, 1)

    ctx.layers = ctx.layers or {}
    ctx.layers.fg = file
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [cl layer="bg"|layer="fg"|layer="all"]
--  Clear specific layer(s). Default: clear all layers.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

function LayerCommands.cl(ctx, params)
    local target = params.layer or "all"

    if target == "all" or target == "bg" then
        local node = layers.find( "bg")
        if node then
            layers.set_layer_visible(node, false)
        end
        if ctx.layers then ctx.layers.bg = nil end
    end

    if target == "all" or target == "fg" then
        local node = layers.find( "fg")
        if node then
            layers.set_layer_visible(node, false)
        end
        if ctx.layers then ctx.layers.fg = nil end
    end
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [image storage="chara/hero.png" layer="fg" x=200 y=100 opacity=255 blend="alpha"]
--  Display image on specified layer with optional position, opacity, blend.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

function LayerCommands.image(ctx, params)
    local file      = resolve_file(params)
    local layerName = params.layer or "fg"

    if not file then
        print("[LayerCmd] image: no file specified")
        return
    end

    local layerType = layers.Type.LAYER_LAYER0
    if layerName == "bg" or layerName == "background" then
        layerType = layers.Type.LAYER_BASE
    elseif layerName == "fg" or layerName == "fore" then
        layerType = layers.Type.LAYER_LAYER0
    elseif layerName == "message" or layerName == "mes" then
        layerType = layers.Type.LAYER_MESSAGE
    end

    local tex = backend.load_texture(file)
    if not tex then
        print("[LayerCmd] image: failed to load " .. file)
        return
    end

    local node = get_or_create_layer( layerName, layerType)
    layers.set_layer_image(node, tex, nil, nil, nil, nil)
    layers.set_layer_visible(node, true)

    if params.x or params.y then
        local x = params.x or 0
        local y = params.y or 0
        layers.move_layer(node, x, y)
    end

    if params.opacity then
        local op = params.opacity
        if op then layers.set_layer_opacity(node, op) end
    end

    if params.blend then
        layers.set_layer_blend(node, params.blend)
    end

    ctx.layers = ctx.layers or {}
    ctx.layers[layerName] = file
end


-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [position layer="fg0" x=0.5 y=0.3 scale=1.0 unit="ndc"]
--  Set a layer's position. x,y in NDC [0-1] unless unit="px".
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

function LayerCommands.position(ctx, params)
    local layerName = params.layer or "fg"
    local x = params.x or 0
    local y = params.y or 0
    local scale = params.scale or 1.0
    local unit = params.unit or "ndc"
    layers.set_position(layerName, x, y, scale, unit)
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  [layopt layer="fg0" opacity=0.8 visible=true blend="multiply"]
--  Batch-set layer visual options.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

function LayerCommands.layopt(ctx, params)
    local layerName = params.layer or "fg"
    local opts = {}
    if params.opacity then
        opts.opacity = params.opacity
    end
    if params.visible ~= nil then
        if type(params.visible) == "string" then
            opts.visible = (params.visible == "true")
        else
            opts.visible = params.visible
        end
    end
    if params.blend then
        opts.blend = params.blend
    end
    layers.set_options(layerName, opts)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  [layfade layer="bg" opacity=0 time=500]
--  Frame-stepped opacity transition for an existing layer (D2.6).
-- ═══════════════════════════════════════════════════════════════════════════

function LayerCommands.layfade(ctx, params)
    local layerName = params.layer or params.name or params[1] or "bg"
    local node = layers.get(layerName)
    if not node then
        print("[LayerCmd] layfade: layer not found: " .. tostring(layerName))
        return
    end
    local target = params.opacity or params.alpha
    if not target then
        print("[LayerCmd] layfade: opacity (0-255) required")
        return
    end
    local duration = params.time or params.duration or 500
    layers.fade_to(node, target, duration)
end

return LayerCommands