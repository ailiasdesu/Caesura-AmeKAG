-- =============================================================================
--  Caesura (AmeKAG)  kag/layout_math.lua
--  Pure declarative layout math for the [layout] command family (v1:
--  hbox / vbox / grid minimal subset).
--
--  This module is a PURE function library: it performs NO layers/backend
--  I/O and mutates no global state, so it can be require()d and unit-tested
--  in isolation (tests/scripts/test_layout_cmds.lua requires it directly).
--
--  Design decision (R107 contract): [layout] is a *calculator*, not a new
--  render path. The command layer (kag/commands/layout.lua) feeds a
--  container definition + registered child slots through these functions to
--  obtain each child's x/y/w/h, then writes those onto the EXISTING layer
--  coordinate attributes (layers.move_layer). Rendering is untouched.
--
--  Coordinate model
--    * The container has an origin (originX, originY). Slots are returned
--      in the container's own coordinate space; the caller adds the origin
--      when writing x/y to a layer node.
--    * padding (or paddingX/paddingY) insets the content box on all sides.
--    * gap is the spacing between adjacent children on the flow axis.
--    * align positions a child on the CROSS axis within its slot box:
--          hbox -> vertical (start=top, middle=center, end=bottom)
--          vbox -> horizontal (start=left, center=mid, end=right)
--          grid -> per-cell, horizontal + vertical (align.h / align.v)
--      When a child has no basis extent on the cross axis (w/h == 0) it
--      fills the slot; otherwise it is shifted per align.
--
--  Items (each child) carry a basis size:
--      w (number)  fixed extent on the horizontal axis (0 = flexible/fill)
--      h (number)  fixed extent on the vertical axis (0 = flexible/fill)
--  On the flow axis an item with size == 0 is FLEXIBLE and receives a share
--  of the leftover content extent (when the container carries an explicit
--  extent) or 0 (when the content extent is derived from fixed items only).
-- =============================================================================

local M = {}

local function to_number(v) return tonumber(v) or 0 end

-- Padded content box derived from a container's origin+size+padding.
-- Returns { x0, y0, w, h } where x0/y0 are the content inset origin.
local function content_box(opts)
    local padX = to_number(opts.paddingX)
    if padX == 0 then padX = to_number(opts.padding) end
    local padY = to_number(opts.paddingY)
    if padY == 0 then padY = to_number(opts.padding) end
    local ow = to_number(opts.w)
    local oh = to_number(opts.h)
    local boxW = math.max(0, ow - 2 * padX)
    local boxH = math.max(0, oh - 2 * padY)
    return { x0 = padX, y0 = padY, w = boxW, h = boxH }
end

local function gap_of(opts)
    local g = to_number(opts.gap)
    if g == 0 then g = to_number(opts.gapX) end
    if g == 0 then g = to_number(opts.gapY) end
    return math.max(0, g)
end

local function axis(item, is_v) return is_v and item.h or item.w end
local function cross(item, is_v) return is_v and item.w or item.h end

-- Distribute the flow-axis extent among items. Fixed sizes are taken
-- first; the leftover is split evenly among flexible (size==0) items.
-- Returns an array of per-item flow extents.
local function distribute_length(items, is_v, totalLength, gap)
    local n = #items
    local fixedSum, flexCount = 0, 0
    for _, it in ipairs(items) do
        local s = axis(it, is_v)
        if s and s > 0 then fixedSum = fixedSum + s else flexCount = flexCount + 1 end
    end
    local gapTotal = (n > 1) and (gap * (n - 1)) or 0
    local leftover = math.max(0, totalLength - fixedSum - gapTotal)
    local flexUnit = (flexCount > 0) and (leftover / flexCount) or 0
    local extents = {}
    for i, it in ipairs(items) do
        local s = axis(it, is_v)
        if s and s > 0 then extents[i] = s else extents[i] = flexUnit end
    end
    return extents
end

-- Distribute the cross-axis extent per item (fill or align-shift).
-- Returns per-item { off, extent } measured along the cross axis.
local function distribute_cross(items, is_v, slotCross, align)
    local offs, extents = {}, {}
    for i, it in ipairs(items) do
        local c = cross(it, is_v)
        if c and c > 0 then
            extents[i] = c
            if align == "end" then
                offs[i] = math.max(0, slotCross - c)
            elseif align == "center" or align == "middle" then
                offs[i] = math.max(0, (slotCross - c) / 2)
            else -- start / default: from origin
                offs[i] = 0
            end
        else
            extents[i] = slotCross
            offs[i] = 0
        end
    end
    return offs, extents
end

-- Shared box layout (hbox with is_v=false, vbox with is_v=true).
local function box_layout(items, opts, is_v)
    items = items or {}
    opts = opts or {}
    local box = content_box(opts)
    local gap = gap_of(opts)
    local align = opts.align or "start"
    local n = #items

    local lengths = distribute_length(items, is_v, is_v and box.h or box.w, gap)
    -- In a box layout every slot spans the FULL content extent on the cross
    -- axis (a vbox row is as wide as its column, an hbox column as tall as
    -- its row). A child with a fixed basis extent on the cross axis is then
    -- aligned (start/center/end) within that slot; otherwise it fills it.
    local slotCross = is_v and box.w or box.h
    local offs, crossExt = distribute_cross(items, is_v, slotCross, align)

    local slots, cursor = {}, is_v and box.y0 or box.x0
    for i = 1, n do
        local flow = lengths[i]
        if is_v then
            slots[i] = { x = box.x0 + offs[i], y = cursor, w = crossExt[i], h = flow }
            cursor = cursor + flow + gap
        else
            slots[i] = { x = cursor, y = box.y0 + offs[i], w = flow, h = crossExt[i] }
            cursor = cursor + flow + gap
        end
    end
    if n == 0 then return { w = 0, h = 0, slots = {} } end
    local flowExtent = cursor - gap
    local cw = is_v and slotCross or flowExtent
    local ch = is_v and flowExtent or slotCross
    return { w = cw, h = ch, slots = slots }
end

-- ═══════════════════════════════════════════════════════════════════════════
--  Public API
-- ═══════════════════════════════════════════════════════════════════════════

--- M.hbox(items, opts) -> { w, h, slots }
function M.hbox(items, opts)
    return box_layout(items, opts, false)
end

--- M.vbox(items, opts) -> { w, h, slots }
function M.vbox(items, opts)
    return box_layout(items, opts, true)
end

--- M.gridrowcol(index, cols) -> { row, col }  (1-based row-major; row/col are 0-based)
function M.gridrowcol(index, cols)
    cols = math.max(1, math.floor(to_number(cols) or 1))
    index = math.max(1, math.floor(to_number(index) or 1))
    return { row = math.floor((index - 1) / cols), col = (index - 1) % cols }
end

--- M.grid(items, opts) -> { w, h, slots }
--  opts: cols (required), gap | gapX|gapY, padding, align = {h,v} | "center", w?, h?
function M.grid(items, opts)
    items = items or {}
    opts = opts or {}
    local n = #items
    local cols = math.max(1, math.floor(to_number(opts.cols) or 1))
    local box = content_box(opts)
    local gap = gap_of(opts)
    local gRows = math.max(1, math.ceil(n / cols))

    local align = opts.align
    if type(align) == "string" and align == "center" then
        align = { h = "center", v = "center" }
    elseif type(align) ~= "table" then
        align = { h = "start", v = "start" }
    end

    local cellW = 0
    local cellH = 0
    if box.w > 0 then cellW = math.max(0, (box.w - (cols - 1) * gap) / cols) end
    if box.h > 0 then cellH = math.max(0, (box.h - (gRows - 1) * gap) / gRows) end
    for _, it in ipairs(items) do
        if cellW <= 0 then cellW = math.max(cellW, to_number(it.w)) end
        if cellH <= 0 then cellH = math.max(cellH, to_number(it.h)) end
    end

    local slots = {}
    for i = 1, n do
        local rc = M.gridrowcol(i, cols)
        local row, col = rc.row, rc.col
        local it = items[i]
        local sx = box.x0 + col * (cellW + gap)
        local sy = box.y0 + row * (cellH + gap)
        local iw, ih = to_number(it.w), to_number(it.h)
        local ox, oy = 0, 0
        local w, h = cellW, cellH
        if iw > 0 and iw < cellW then
            w = iw
            if align.h == "end" then ox = cellW - iw
            elseif align.h == "center" then ox = (cellW - iw) / 2 end
        end
        if ih > 0 and ih < cellH then
            h = ih
            if align.v == "end" then oy = cellH - ih
            elseif align.v == "center" then oy = (cellH - ih) / 2 end
        end
        slots[i] = { x = sx + ox, y = sy + oy, w = w, h = h }
    end

    local gw = box.w > 0 and box.w or (cellW * cols + gap * (cols - 1))
    local gh = box.h > 0 and box.h or (cellH * gRows + gap * (gRows - 1))
    if n == 0 then gw, gh = 0, 0 end
    return { w = gw, h = gh, slots = slots }
end

--- M.measure(kind, items, opts) -> { w, h, slots }  -- full resolve; measure+slots
function M.measure(kind, items, opts)
    if kind == "hbox" then return M.hbox(items, opts) end
    if kind == "vbox" then return M.vbox(items, opts) end
    if kind == "grid" then return M.grid(items, opts) end
    return { w = 0, h = 0, slots = {} }
end

--- M.slot_rect(kind, index, items, opts) -> { x, y, w, h } or nil (out of range)
function M.slot_rect(kind, index, items, opts)
    local res = M.measure(kind, items, opts)
    index = math.floor(to_number(index) or 1)
    if res.slots and res.slots[index] then return res.slots[index] end
    return nil
end

return M
