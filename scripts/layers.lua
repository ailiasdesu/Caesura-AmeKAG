-- ═══════════════════════════════════════════════════════════════════════════
--  Caesura (AmeKAG) — layers.lua
--  Layer tree manager. Full implementation per spec [2.1].
--  Each layer supports: z-order, blend mode, visibility, opacity, position,
--  size, clipping, hit testing, dirty tracking, per-layer RTT.
--  Render pipeline: DFS traversal → z-order sort → blend child RTs →
--  submit via backend.submit_batch → flush to screen.
--  Uses pool.lua for per-frame batch command tables (reduces GC).
--  Krkrz reference: LayerIntf.h, LayerManager.h, drawable.h (28 blend types)
-- ═══════════════════════════════════════════════════════════════════════════

local backend   = require("backend")
local rtt       = require("rtt")
local blend_lib = require("blend")

local Layers = {}

-- ═══════════════════════════════════════════════════════════════════════════
--  LayerType — scene-layer role enum (spec [2.1])
-- ═══════════════════════════════════════════════════════════════════════════

Layers.Type = {
    LAYER_BASE    = 1,  -- 背景层
    LAYER_LAYER0  = 2,  -- 立绘层0
    LAYER_LAYER1  = 3,  -- 立绘层1
    LAYER_FORE    = 4,  -- 前景层
    LAYER_UI      = 5,  -- UI 层
    LAYER_MESSAGE = 6,  -- 消息层
    LAYER_EFFECT  = 7,  -- 特效层
}

Layers.TypeName = {
    [1] = "base", [2] = "layer0", [3] = "layer1",
    [4] = "fore", [5] = "ui", [6] = "message", [7] = "effect",
}

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal state
-- ═══════════════════════════════════════════════════════════════════════════

local layerMap  = {}   -- id → LayerNode lookup
local rootNode  = nil  -- scene root
local nextView  = 1    -- bgfx View ID allocator
local maxView   = 256  -- bgfx View limit (bgfx caps at 256)

-- ═══════════════════════════════════════════════════════════════════════════
--  Node constructor
-- ═══════════════════════════════════════════════════════════════════════════

local function newLayerNode(id, config)
    config = config or {}
    return {
        id          = id,
        name        = config.name or config.tag or ("layer_" .. id),
        parent      = nil,
        children    = {},             -- sorted by z-order ascending
        layer_type  = config.layer_type or Layers.Type.LAYER_FORE,
        x           = config.x or 0,
        y           = config.y or 0,
        w           = config.w or 0,
        h           = config.h or 0,
        visible     = (config.visible == nil) or config.visible,
        opacity     = config.opacity or 255,
        blend_mode  = config.blend_mode or "alpha",
        rt          = config.rt or nil,      -- RTT handle
        view_id     = nil,                   -- bgfx View ID
        dirty       = true,
        dirty_rect  = nil,                   -- {x,y,w,h} sub-region
        z           = config.z or 0,
        -- visual state
        scaleX      = 1.0,  scaleY = 1.0,
        rotation    = 0,
        originX     = 0,    originY = 0,
        alpha       = (config.opacity or 255) / 255.0,
        -- clipping (pixels in parent space)
        clipX       = config.clipX, clipY = config.clipY,
        clipW       = config.clipW, clipH = config.clipH,
        -- image sub-rect
        imgX        = nil, imgY = nil, imgW = nil, imgH = nil,
        -- texture tracking for blend
        _prev_rt    = nil,
        -- vfx state
        quake       = { active = false, amplitude_x = 0, amplitude_y = 0 },
        shake       = { active = false, offset_x = 0, offset_y = 0 },
        fade        = { active = false, alpha = 255 },
        -- application data
        tag         = config.tag or nil,
        userdata    = config.userdata or nil,
    }
end

-- ═══════════════════════════════════════════════════════════════════════════
--  z-order helpers
-- ═══════════════════════════════════════════════════════════════════════════

local function sortChildrenByZ(node)
    if not node or not node.children then return end
    table.sort(node.children, function(a, b) return a.z < b.z end)
end

local function insertSortedByZ(children, node)
    local idx = 1
    while idx <= #children and (children[idx].z or 0) <= (node.z or 0) do
        idx = idx + 1
    end
    table.insert(children, idx, node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.init() — reset the tree
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.init()
    layerMap = {}
    rootNode = nil
    nextView = 1
    Layers.get_root()
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.get_root() → root LayerNode (lazy init)
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.get_root()
    if not rootNode then
        rootNode = {
            id = "_root", parent = nil, children = {},
            layer_type = Layers.Type.LAYER_BASE,
            x = 0, y = 0, w = 0, h = 0,
            visible = true, opacity = 255,
            blend_mode = "opaque", z = -9999,
            rt = nil, view_id = 0,
            dirty = true, dirty_rect = nil,
            scaleX = 1.0, scaleY = 1.0, rotation = 0,
            alpha = 1.0, quake = {}, shake = {},
        }
        layerMap["_root"] = rootNode
    end
    return rootNode
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.count() → integer
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.count()
    local n = 0
    for _ in pairs(layerMap) do n = n + 1 end
    return n
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.get_layer(id) → LayerNode | nil
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.get_layer(id)
    return layerMap[id]
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.find(pred) → LayerNode | nil
--    pred: function(node)→boolean, or string tag
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.find(pred)
    if type(pred) == "string" then
        local tag = pred
        pred = function(n) return n.tag == tag end
    end
    for _, node in pairs(layerMap) do
        if pred(node) then return node end
    end
    return nil
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.get(layer_name) → LayerNode | nil
--    Look up a layer by its name field (set at construction).
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.get(layer_name)
    if not layer_name then return nil end
    for _, node in pairs(layerMap) do
        if node.name == layer_name then return node end
    end
    return nil
end


-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.forEach(fn) — iterate all layers
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.forEach(fn)
    for id, node in pairs(layerMap) do
        fn(id, node)
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.add_layer(parent, config) → LayerNode
--    Creates a new layer node under parent (defaults to root), allocates
--    an RTT, assigns a bgfx View ID, inserts in z-order, marks dirty.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.add_layer(parent, config)
    config = config or {}
    local id = config.id or ("layer_" .. tostring(Layers.count() + 1))
    parent = parent or Layers.get_root()

    local node = newLayerNode(id, config)
    node.parent = parent

    -- allocate RTT if size is known
    local nw = node.w or 0
    local nh = node.h or 0
    if nw > 0 and nh > 0 then
        node.rt = rtt.create(nw, nh)
    end

    -- assign bgfx View ID
    if nextView >= maxView then
        error("[layers] bgfx View ID exhausted (max=" .. tostring(maxView) .. ")")
    end
    node.view_id = nextView
    nextView = nextView + 1

    -- insert in z-order
    parent.children = parent.children or {}
    insertSortedByZ(parent.children, node)

    layerMap[id] = node
    Layers.mark_dirty(node)
    return node
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.remove_layer(node) — recursive tree-aware removal
--    Also frees the RTT and clears the layerMap entry.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.remove_layer(node)
    if not node then return false end

    -- detach from parent
    if node.parent and node.parent.children then
        for i, child in ipairs(node.parent.children) do
            if child == node then
                table.remove(node.parent.children, i)
                break
            end
        end
        Layers.mark_dirty(node.parent)
    end

    -- recursively remove children (copy list to avoid mutation during iteration)
    if node.children then
        local children = {}
        for _, c in ipairs(node.children) do table.insert(children, c) end
        for _, c in ipairs(children) do
            Layers.remove_layer(c)
        end
    end

    -- free RTT
    if node.rt then
        rtt.destroy(node.rt)
        node.rt = nil
    end

    layerMap[node.id] = nil
    return true
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_z(node, z) — reorder and mark dirty
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_z(node, z)
    if not node then return end
    node.z = z
    if node.parent and node.parent.children then
        sortChildrenByZ(node.parent)
    end
    Layers.mark_dirty(node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.mark_dirty(node, rect) — mark node + all ancestors dirty
--    Called by transform, blend, vfx, and external mutations.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.mark_dirty(node, rect)
    if not node then return end
    node.dirty = true
    if rect then
        node.dirty_rect = { x = rect.x, y = rect.y, w = rect.w, h = rect.h }
    end
    -- propagate upward to root
    local p = node.parent
    while p do
        p.dirty = true
        p = p.parent
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Internal: clear dirty flags recursively (post-render)
-- ═══════════════════════════════════════════════════════════════════════════

local function clearDirtyRecursive(node)
    if not node then return end
    node.dirty = false
    node.dirty_rect = nil
    for _, child in ipairs(node.children or {}) do
        clearDirtyRecursive(child)
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.get_world_pos(node) → x, y — absolute screen coordinates
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.get_world_pos(node)
    local x, y = node.x or 0, node.y or 0
    local p = node.parent
    while p do
        x = x + (p.x or 0)
        y = y + (p.y or 0)
        p = p.parent
    end
    return x, y
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.get_world_rect(node) → {x,y,w,h} — absolute screen rect
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.get_world_rect(node)
    local wx, wy = Layers.get_world_pos(node)
    return { x = wx, y = wy, w = node.w or 0, h = node.h or 0 }
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.hit_test(x, y) → LayerNode
--    Walks the tree depth-first from the root, returning the deepest
--    visible leaf node whose bounding rect contains (x, y).
--    Higher z-order children are checked last (topmost → first hit).
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.hit_test(x, y)
    local function hitRecursive(node, sx, sy)
        if not node or not node.visible then return nil end
        local nx = sx + (node.x or 0)
        local ny = sy + (node.y or 0)
        local nw = node.w or 0
        local nh = node.h or 0

        -- check children from topmost (highest z) to bottommost
        local children = node.children or {}
        for i = #children, 1, -1 do
            local hit = hitRecursive(children[i], nx, ny)
            if hit then return hit end
        end

        -- self hit: point must be inside bounding rect
        if nw > 0 and nh > 0 then
            if x >= nx and x < nx + nw and y >= ny and y < ny + nh then
                return node
            end
        end
        return nil
    end

    return hitRecursive(Layers.get_root(), 0, 0)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.emit_text(layer, content) — render text via backend
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.emit_text(layer, content)
    if not layer or not content then return end
    local wr = Layers.get_world_rect(layer)
    backend.render_text(content, wr.x, wr.y)
    Layers.mark_dirty(layer)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.capture_snapshot() → table — deep-copy state (backlog / save)
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.capture_snapshot()
    local snap = { layers = {} }
    for id, node in pairs(layerMap) do
        if id ~= "_root" then
            snap.layers[id] = {
                id         = node.id,
                x          = node.x,
                y          = node.y,
                w          = node.w,
                h          = node.h,
                visible    = node.visible,
                opacity    = node.opacity,
                alpha      = node.alpha,
                blend_mode = node.blend_mode,
                z          = node.z,
                scaleX     = node.scaleX,
                scaleY     = node.scaleY,
                rotation   = node.rotation,
                clipX      = node.clipX,
                clipY      = node.clipY,
                clipW      = node.clipW,
                clipH      = node.clipH,
                layer_type = node.layer_type,
                tag        = node.tag,
            }
        end
    end
    return snap
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.restore_snapshot(snap) — restore from snapshot, mark dirty
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.restore_snapshot(snap)
    if not snap or not snap.layers then return end
    for id, data in pairs(snap.layers) do
        local node = layerMap[id]
        if node then
            for k, v in pairs(data) do node[k] = v end
            node.dirty = true
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.render() — main render pipeline (spec [2.1])
--
--  1. DFS traversal of the scene tree starting at root.
--  2. For each visible node with dirty flag:
--     a. Render children first (bottom-up composition).
--     b. Compose child RTs onto current node RT via blend mode.
--     c. Collect batch commands.
--  3. Submit the unified batch through backend.submit_batch.
--  4. Clear all dirty flags.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.render()
    local root = Layers.get_root()
    if not root then return end

    local batch = pool.eventTablePool:acquire()
    batch.commands = pool.eventTablePool:acquire()
    batch.layer_count = 0

    local function renderNode(node, parent_wx, parent_wy)
        if not node or not node.visible then return end

        local use_x = (node.pos_x ~= nil) and node.pos_x or node.x
        local use_y = (node.pos_y ~= nil) and node.pos_y or node.y
        local wx = parent_wx + (use_x or 0)
        local wy = parent_wy + (use_y or 0)

        -- apply quake / shake offsets
        local qx, qy = 0, 0
        if node.quake and node.quake.active then
            qx = node.quake.offset_x or 0
            qy = node.quake.offset_y or 0
        end
        if node.shake and node.shake.active then
            qx = qx + (node.shake.offset_x or 0)
            qy = qy + (node.shake.offset_y or 0)
        end
        wx = wx + qx
        wy = wy + qy

        -- render children first (bottom → top)
        for _, child in ipairs(node.children or {}) do
            renderNode(child, wx, wy)
        end

        -- only emit commands for dirty nodes that own an RT
        if node.dirty and node.rt and node.view_id then
            batch.commands[#batch.commands + 1] = {
                view_id    = node.view_id,
                rt         = node.rt,
                x          = wx,
                y          = wy,
                w          = node.w or 0,
                h          = node.h or 0,
                blend_mode = node.blend_mode or "alpha",
                opacity    = node.opacity or 255,
                scaleX     = node.scale or node.scaleX or 1.0,
                scaleY     = node.scale or node.scaleY or 1.0,
                rotation   = node.rotation or 0,
                clipX      = node.clipX,
                clipY      = node.clipY,
                clipW      = node.clipW,
                clipH      = node.clipH,
            }
            batch.layer_count = batch.layer_count + 1
        end
    end

    renderNode(root, 0, 0)

    if batch.layer_count > 0 then
        backend.submit_batch(batch)
    end

    clearDirtyRecursive(root)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.compose_children(node) — blend child RTs into node RT
--    Called internally after children are rendered.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.compose_children(node)
    if not node or not node.children then return end
    for _, child in ipairs(node.children) do
        if child.dirty and child.rt and node.rt then
            blend_lib.blend_into(
                node.rt,
                child.rt,
                child.blend_mode or "alpha",
                child.opacity or 255,
                child.x or 0,
                child.y or 0,
                child.w or 0,
                child.h or 0,
                child.view_id
            )
        end
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_layer_image(node, tex_id, ix, iy, iw, ih)
--    Assign a texture to a layer. Auto-set size from image rect.
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_layer_image(node, tex_id, ix, iy, iw, ih)
    if not node then return end
    node.rt = tex_id
    node.imgX = ix; node.imgY = iy; node.imgW = iw; node.imgH = ih
    if iw and ih then
        node.w = iw
        node.h = ih
    end
    Layers.mark_dirty(node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_layer_visible(node, visible)
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_layer_visible(node, visible)
    if not node then return end
    if node.visible ~= visible then
        node.visible = visible
        Layers.mark_dirty(node.parent or node)
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_layer_opacity(node, opacity_0_255)
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_layer_opacity(node, opacity)
    if not node then return end
    node.opacity = math.max(0, math.min(255, tonumber(opacity) or 255))
    node.alpha = node.opacity / 255.0
    Layers.mark_dirty(node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_layer_blend(node, blend_mode)
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_layer_blend(node, blend_mode)
    if not node then return end
    node.blend_mode = blend_mode
    Layers.mark_dirty(node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.move_layer(node, x, y) — absolute position; dirties the node
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.move_layer(node, x, y)
    if not node then return end
    if x ~= nil then node.x = x end
    if y ~= nil then node.y = y end
    Layers.mark_dirty(node)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.resize_layer(node, w, h) — resize and reallocate RTT
-- ═══════════════════════════════════════════════════════════════════════════


-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_position(layer_name, x, y, scale, unit)
--    Set layer position. x,y are NDC [0-1] by default, or pixel when
--    unit="px" (assumes 1280×720 reference resolution).
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_position(layer_name, x, y, scale, unit)
    if unit == "px" then
        x = x / 1280
        y = y / 720
    end
    local l = Layers.get(layer_name)
    if l then
        l.pos_x = x; l.pos_y = y; l.scale = scale or 1.0
        Layers.mark_dirty(l)
    end
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Layers.set_options(layer_name, opts)
--    Batch-set layer options: opacity (0.0-1.0), visible (bool), blend (string).
-- ═══════════════════════════════════════════════════════════════════════════

function Layers.set_options(layer_name, opts)
    local l = Layers.get(layer_name)
    if not l then return end
    if opts.opacity then
        local op = tonumber(opts.opacity)
        if op then
            -- opacity in [0.0, 1.0] range from KAG
            local a = math.max(0.0, math.min(1.0, op))
            l.opacity = math.floor(a * 255)
            l.alpha = a
        end
    end
    if opts.visible ~= nil then
        if l.visible ~= opts.visible then
            l.visible = opts.visible
        end
    end
    if opts.blend then
        l.blend_mode = opts.blend
    end
    Layers.mark_dirty(l)
end

-- =============================================================================
--  Layers.restore_text_state(text_state)
--  Restore text rendering position after save/load.
-- =============================================================================

function Layers.restore_text_state(text_state)
    if not text_state then return end
    if backend.text_set_state then
        backend.text_set_state(text_state.line or 1, text_state.char_offset or 0)
    end
end

function Layers.resize_layer(node, w, h)
    if not node then return end
    if w ~= nil then node.w = w end
    if h ~= nil then node.h = h end
    if node.rt then rtt.destroy(node.rt) end
    node.rt = rtt.create(node.w, node.h)
    Layers.mark_dirty(node)
end

return Layers