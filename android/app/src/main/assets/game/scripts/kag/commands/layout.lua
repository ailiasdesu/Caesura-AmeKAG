-- =============================================================================
--  Caesura (AmeKAG)  kag/commands/layout.lua
--  Declarative [layout] container family (v1: hbox / vbox / grid).
--
--  [layout] is a COMPUTER, not a render layer: the declaring tag (+ slot /
--  place sub-tags) resolve each child's x/y/w/h from a pure layout spec
--  (kag/layout_math.lua) and write those onto the EXISTING layer coordinate
--  attributes via layers.move_layer. The render pipeline is untouched, and
--  the output composes with [position] / [tween] (which read/write the same
--  node.x / node.y).
--
--  Command family (one implementation table, registered under three names):
--    [layout name=X kind=hbox|vbox|grid gap= padding= align= cols=
--            w= h= x= y=]                      declares/updates a container.
--    [layout_slot parent=X index=N size="WxH" layer=Y]
--                                               registers element Y at slot N
--                                               (with basis size WxH).
--    [layout_place parent=X layer=Y x=DX y=DY]  places element Y at an
--                                               absolute offset inside the
--                                               container frame.
--  Each tag is its OWN schema command (the fused "[layout slot ...]" spelling
--  is not used: the [layout] contract requires kind at coerce time, so a
--  positional discriminator can never reach the handler). All three register
--  as separate dispatch keys in kag.lua.
--
--  Container registry: ctx.layouts[name] holds the container's definition,
--  its slot geometry and the ordered item list. Slots are recomputed
--  (and every registered element re-placed) whenever a tag mutates the
--  container or its items.
-- =============================================================================

local layers = require("layers")
local math2  = require("kag.layout_math")

local schema = require("kag.schema")

-- ─────────────────────────────────────────────────────────────────────────────
--  Schema contracts (typed + clamped via kag/schema)
-- ─────────────────────────────────────────────────────────────────────────────
schema.define("layout", {
    _meta = {
        category = "layer",
        blocking = false,
        desc = "declare an hbox/vbox/grid container that computes child x/y (calculator, not a render layer)",
    },
    name   = { type = "string", required = true },
    kind   = { type = "enum", required = true, values = { "hbox", "vbox", "grid" } },
    gap    = { type = "number", default = 0, min = 0, max = 8192 },
    padding  = { type = "number", default = 0, min = 0, max = 8192 },
    paddingX = { type = "number", min = 0, max = 8192 },
    paddingY = { type = "number", min = 0, max = 8192 },
    align  = { type = "string", default = "start" },
    cols   = { type = "number", min = 1, max = 128 },
    w      = { type = "number", min = 0, max = 8192 },
    h      = { type = "number", min = 0, max = 8192 },
    x      = { type = "number", default = 0 },
    y      = { type = "number", default = 0 },
    layer  = { type = "string" },   -- optional container anchor layer name
})

schema.define("layout_slot", {
    _meta = {
        category = "layer",
        blocking = false,
        desc = "register an element layer into a slot of a declared [layout] container",
    },
    parent = { type = "string", required = true },
    layer  = { type = "string", required = true },
    index  = { type = "number", min = 1, max = 1024 },
    size   = { type = "string" },   -- "WxH" basis size; e.g. "90x30"
})

schema.define("layout_place", {
    _meta = {
        category = "layer",
        blocking = false,
        desc = "place an element layer at an absolute offset inside a [layout] container frame",
    },
    parent = { type = "string", required = true },
    layer  = { type = "string", required = true },
    x      = { type = "number", default = 0 },
    y      = { type = "number", default = 0 },
    w      = { type = "number", min = 0, max = 8192 },
    h      = { type = "number", min = 0, max = 8192 },
})

local LayoutCommands = {}

-- Parse a "WxH" basis-size string into { w, h }. Returns nil when invalid.
local function parse_size(s)
    if type(s) ~= "string" then return nil end
    local w, h = s:match("^%s*(%d+)%s*[xX]%s*(%d+)%s*$")
    if not w then return nil end
    return { w = tonumber(w), h = tonumber(h) }
end

-- Resolve a layer node by name (creating it as a bare layer under root if
-- missing) so layout can write coordinates onto any named element.
local function resolve_layer(name)
    if type(name) ~= "string" or #name == 0 then return nil end
    return layers.get(name) or layers.find(name)
        or layers.add_layer(layers.get_root(), { name = name, id = name, tag = name })
end

-- Recompute a container's slots from its items and store them.
local function recompute(cont)
    local items = cont.items or {}
    local opts = {
        gap = cont.gap or 0,
        padding = cont.padding or 0,
        paddingX = cont.paddingX,
        paddingY = cont.paddingY,
        align = cont.align,
        cols = cont.cols,
        w = cont.w,
        h = cont.h,
    }
    local basis = {}
    for _, it in ipairs(items) do basis[#basis + 1] = { w = it.w, h = it.h } end
    local res = math2.measure(cont.kind, basis, opts)
    cont.computedW, cont.computedH = res.w, res.h
    cont.slots = res.slots or {}
end

-- Re-apply every registered element to its slot rect + container origin.
local function apply_container(cont)
    if not cont then return end
    local ox = cont.originX or 0
    local oy = cont.originY or 0
    for i, it in ipairs(cont.items or {}) do
        local slot = cont.slots and cont.slots[i]
        if slot and it.layer then
            local node = resolve_layer(it.layer)
            if node then layers.move_layer(node, ox + slot.x, oy + slot.y) end
        end
    end
end

local function get_container(ctx, name)
    if not ctx or type(ctx.layouts) ~= "table" then return nil end
    return ctx.layouts[name]
end

local function new_container(name, params)
    return {
        name = name,
        kind = params.kind,
        gap = params.gap or 0,
        padding = params.padding or 0,
        paddingX = params.paddingX,
        paddingY = params.paddingY,
        align = params.align or "start",
        cols = params.cols,
        w = params.w,
        h = params.h,
        originX = params.x or 0,
        originY = params.y or 0,
        layer = params.layer,
        items = {},
        slots = {},
        computedW = 0,
        computedH = 0,
    }
end

-- ─────────────────────────────────────────────────────────────────────────────
--  [layout name= kind= ...] — declare / update a container
-- ─────────────────────────────────────────────────────────────────────────────
function LayoutCommands.layout(ctx, params)
    local p = params or {}
    local name = p.name
    if not name then
        print("[layout] missing name")
        return
    end
    ctx.layouts = ctx.layouts or {}
    local cont = ctx.layouts[name]
    if not cont then
        cont = new_container(name, p)
        ctx.layouts[name] = cont
    else
        -- Update mutable fields (kind/gap/padding/align/cols/w/h/origin).
        cont.kind = p.kind
        cont.gap = p.gap or 0
        cont.padding = p.padding or 0
        cont.paddingX = p.paddingX
        cont.paddingY = p.paddingY
        cont.align = p.align or "start"
        cont.cols = p.cols
        cont.w = p.w
        cont.h = p.h
        cont.originX = p.x or 0
        cont.originY = p.y or 0
        if p.layer ~= nil then cont.layer = p.layer end
    end
    recompute(cont)
    apply_container(cont)
    return cont
end

-- ─────────────────────────────────────────────────────────────────────────────
--  [layout_slot parent= index= size="WxH" layer=] — register element at slot
-- ─────────────────────────────────────────────────────────────────────────────
function LayoutCommands.layout_slot(ctx, params)
    local p = params or {}
    local cont = get_container(ctx, p.parent)
    if not cont then
        print("[layout_slot] container not found: " .. tostring(p.parent)
            .. " (declare [layout name=...] first)")
        return
    end
    local size = parse_size(p.size)
    local item = { layer = p.layer, w = size and size.w or 0, h = size and size.h or 0 }

    cont.items = cont.items or {}
    local n = #(cont.items or {})
    local index
    if p.index ~= nil then
        index = math.floor(p.index)
        -- Out-of-range index clamps to the contiguous range: <1 -> first,
        -- > len+1 -> append at end (no crash, no holes).
        if index < 1 then index = 1 end
        if index > n + 1 then index = n + 1 end
    else
        index = n + 1
    end
    if index <= n then
        -- Replace the item already at this slot (re-flowed in place).
        cont.items[index] = item
    else
        -- Append (contiguous tail). index == n+1 normally.
        table.insert(cont.items, item)
    end

    recompute(cont)
    apply_container(cont)
    return item
end

-- ─────────────────────────────────────────────────────────────────────────────
--  [layout_place parent= layer= x= y= w= h=] — absolute place inside frame
-- ─────────────────────────────────────────────────────────────────────────────
function LayoutCommands.layout_place(ctx, params)
    local p = params or {}
    local cont = get_container(ctx, p.parent)
    if not cont then
        print("[layout_place] container not found: " .. tostring(p.parent))
        return
    end
    local node = resolve_layer(p.layer)
    if not node then return end
    local ox = cont.originX or 0
    local oy = cont.originY or 0
    layers.move_layer(node, ox + (p.x or 0), oy + (p.y or 0))
    if p.w then node.w = p.w end
    if p.h then node.h = p.h end
    if node.w or node.h then layers.mark_dirty(node) end
    return node
end

return LayoutCommands
