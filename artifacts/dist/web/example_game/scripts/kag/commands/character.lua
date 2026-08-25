-- =============================================================================
--  Caesura (AmeKAG) — kag/commands/character.lua
--  Phase 4: KAG3 character convenience tags — [csp], [csd], [csl]
--  Character show / delete / move commands layered on top of the layer tree
--  (a KAG3-compat veneer over the underlying [image]/[position]/[moveto]
--  layer pipeline, per spec [2.1]).
--
--  Semantics (KAG3 compatibility):
--    [csp name=chara layer=0 x=320 y=240]  show a character: ensure the layer
--        exists and is visible, assign assets/char/<name>.png (or an explicit
--        storage/file/path override, mirroring [image] path resolution) as the
--        layer image, and set its position.
--    [csd name=chara layer=0]  hide/clear the character on the layer (mirrors
--        [cl]): the layer is made invisible and its texture is dropped.
--    [csl name=chara layer=0 x=340 y=240]  move a character layer to a new
--        position without changing its visibility (mirrors [moveto]/[position]
--        positioning via Layers.move_layer).
--
--  All calls route through layers.lua + backend.lua, exactly like the layer
--  commands. Layer names are normalized to strings (both numeric and named
--  forms are accepted: "0" vs 0 vs "fg0").
-- =============================================================================

local backend = require("backend")
local layers  = require("layers")

-- -----------------------------------------------------------------------------
--  Neo-Genesis contracts: typed + clamped via kag/schema. Category "layer"
--  groups these with the underlying layer commands; blocking=false (none of
--  these wait on completion).
-- -----------------------------------------------------------------------------

local schema = require("kag.schema")
schema.define("csp", {
    _meta = { category = "layer", blocking = false,
        desc = "KAG3-compatible csp command: show a character image on a layer (default assets/char/<name>.png at 0,0)" },
    name    = { type = "string", required = true, positional_index = 1 },  -- character id / asset stem
    layer   = { type = "string", default = "0" },
    x       = { type = "number", default = 0 },
    y       = { type = "number", default = 0 },
    storage = { type = "file" },   -- optional image-style path override
    file    = { type = "file" },
    path    = { type = "string" },
})
schema.define("csd", {
    _meta = { category = "layer", blocking = false,
        desc = "KAG3-compatible csd command: hide/remove a character on a layer" },
    name    = { type = "string", required = true, positional_index = 1 },
    layer   = { type = "string", default = "0" },
})
schema.define("csl", {
    _meta = { category = "layer", blocking = false,
        desc = "KAG3-compatible csl command: move a character layer (no visibility change)" },
    name    = { type = "string", required = true, positional_index = 1 },
    layer   = { type = "string", default = "0" },
    x       = { type = "number", default = 0 },
    y       = { type = "number", default = 0 },
})

local CharacterCommands = {}

-- Internal: resolve the character image path. Mirrors [image]'s resolve_file
-- (storage > path > file override) plus mod resolution; when no explicit
-- path is given, falls back to the KAG3 convention assets/char/<name>.png.
local function resolve_character_file(params, chara)
    local f = params.storage or params.path or params.file
    if type(f) ~= "string" or #f == 0 then
        f = "assets/char/" .. tostring(chara) .. ".png"
    end
    -- Mod resolution: enabled mods may override base assets
    -- (mods/<name>/<path>); falls back to the base path.
    if type(f) == "string" and #f > 0 then
        f = require("mods").resolve(f)
    end
    return f
end

-- Internal: resolve a layer node by name (numeric or string form), creating
-- it under the root if missing — same "ensure" semantics as layer.lua's
-- get_or_create_layer, with id=name so both Layers.get and Layers.find(tag)
-- resolve it. Character layers default to size 0 (the RTT/layout is driven
-- by the image once assigned).
local function get_or_create_layer(layerName)
    local node = layers.find(layerName) or layers.get(layerName)
    if not node then
        local root = layers.get_root()
        node = layers.add_layer(root, {
            name = layerName, id = layerName, tag = layerName,
            z = 1, x = 0, y = 0, w = 0, h = 0, visible = true,
        })
    end
    return node
end

-- [csp name=chara layer=0 x=320 y=240]
-- Show a character: ensure the layer exists and is visible, assign the
-- resolved image, and place it at (x, y). Re-showing the same layer with a
-- different name updates the image.
function CharacterCommands.csp(ctx, params)
    local chara = params.name or params[1]
    if not chara then
        print("[CharCmd] csp: chara name required")
        return
    end
    local layerName = tostring(params.layer)
    local file     = resolve_character_file(params, chara)

    local tex = backend.load_texture(file)
    if not tex then
        print("[CharCmd] csp: failed to load " .. file)
        return
    end

    local node = get_or_create_layer(layerName)
    layers.set_layer_image(node, tex, nil, nil, nil, nil)
    layers.set_layer_visible(node, true)
    layers.move_layer(node, params.x or 0, params.y or 0)

    ctx.layers = ctx.layers or {}
    ctx.layers[layerName] = file
    ctx.characters = ctx.characters or {}
    ctx.characters[layerName] = { chara = chara, file = file }
end

-- [csd name=chara layer=0]
-- Hide/remove the character on the layer. Mirrors [cl] (hide) plus [ld]'s
-- texture drop so the layer no longer carries a stale image; the dedup state
-- is cleared so the next [csp] re-asserts cleanly.
function CharacterCommands.csd(ctx, params)
    local layerName = tostring(params.layer)
    local node = layers.find(layerName) or layers.get(layerName)
    if node then
        layers.set_layer_visible(node, false)
        node.tex = nil
        node.texture = nil
    end
    if ctx.layers then ctx.layers[layerName] = nil end
    if ctx.characters then ctx.characters[layerName] = nil end
end

-- [csl name=chara layer=0 x=340 y=240]
-- Move a character layer to (x, y) WITHOUT changing its visibility — mirrors
-- [moveto]/[position] positioning via Layers.move_layer. No-op (with a
-- diagnostic) when the layer has not been shown yet.
function CharacterCommands.csl(ctx, params)
    local layerName = tostring(params.layer)
    local node = layers.find(layerName) or layers.get(layerName)
    if not node then
        print("[CharCmd] csl: layer not shown: " .. layerName)
        return
    end
    layers.move_layer(node, params.x or 0, params.y or 0)
end

return CharacterCommands
