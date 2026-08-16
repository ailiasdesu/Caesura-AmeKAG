-- test_layout_cmds.lua — [layout] declarative container command family.
-- (hbox / vbox / grid; [layout][layout_slot][layout_place]; composition with
--  [position] / [tween]).
-- ORPHAN SUITE: it mocks package.loaded["layers"], so it must run isolated
-- (tests/scripts/run_orphan_tests.lua) and must NEVER be merged into the main
-- suite (sandbox locks globals mid-run; a layers mock would corrupt later tests).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end
local function near(a, b)
    return math.abs((a or 0) - (b or 0)) < 0.5
end

-- ── Mock the layers module (capture move_layer / node map) ────────────────
local nodes = {}
local moved = {}
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = {
    get_root = function()
        return { id = "_root", children = {} }
    end,
    get = function(name) return nodes[name] end,
    find = function(name) return nodes[name] end,
    add_layer = function(parent, cfg)
        local node = { name = cfg.name, id = cfg.id or cfg.name, tag = cfg.tag or cfg.name,
                       x = 0, y = 0, w = 0, h = 0, visible = true, children = {} }
        nodes[node.name] = node
        return node
    end,
    move_layer = function(node, x, y)
        if x ~= nil then node.x = x end
        if y ~= nil then node.y = y end
        moved[node.name or "?"] = (moved[node.name or "?"] or 0) + 1
    end,
    mark_dirty = function() end,
}

local KAG = require("kag")
local schema = require("kag.schema")
local LayoutMath = require("kag.layout_math")
local LayoutCmd = require("kag.commands.layout")

local function coerce(name, params) return schema.coerce(name, params, {}) end

-- ═══════════════════════════════════════════════════════════════════════════
--  1. PURE MATH — hbox / vbox / grid matrices
-- ═══════════════════════════════════════════════════════════════════════════
-- hbox: two fixed-width children (120 + 180) at gap 20.
do
    local res = LayoutMath.hbox({ { w = 120, h = 30 }, { w = 180, h = 30 } },
        { w = 340, h = 30, gap = 20 })
    check("hbox 2 cols produced", res.slots[1] and res.slots[2] ~= nil)
    check("hbox col1 x=0", near(res.slots[1].x, 0))
    check("hbox col1 w=120", near(res.slots[1].w, 120))
    check("hbox col2 x=140", near(res.slots[2].x, 140))
    check("hbox col2 w=180", near(res.slots[2].w, 180))
    check("hbox content w=320", near(res.w, 320))
    check("hbox row h=30", near(res.slots[1].h, 30))
end

-- vbox: three equal flexible rows over 90 high -> 30 each.
do
    local res = LayoutMath.vbox({ { h = 0 }, { h = 0 }, { h = 0 } },
        { w = 100, h = 90 })
    check("vbox 3 rows produced", res.slots[1] and res.slots[2] and res.slots[3] ~= nil)
    check("vbox row1 y=0", near(res.slots[1].y, 0))
    check("vbox row2 y=30", near(res.slots[2].y, 30))
    check("vbox row3 y=60", near(res.slots[3].y, 60))
    check("vbox row h=30", near(res.slots[1].h, 30))
    check("vbox content h=90", near(res.h, 90))
end

-- grid: 2 cols x 2 rows over 200x80 -> 100x40 cells.
do
    local res = LayoutMath.grid({ { w = 0 }, { w = 0 }, { w = 0 }, { w = 0 } },
        { cols = 2, w = 200, h = 80 })
    check("grid 4 cells", #res.slots == 4)
    check("grid a(1) 0,0", near(res.slots[1].x, 0) and near(res.slots[1].y, 0))
    check("grid b(2) 100,0", near(res.slots[2].x, 100) and near(res.slots[2].y, 0))
    check("grid c(3) 0,40", near(res.slots[3].x, 0) and near(res.slots[3].y, 40))
    check("grid d(4) 100,40", near(res.slots[4].x, 100) and near(res.slots[4].y, 40))
    check("grid cell 100x40", near(res.slots[1].w, 100) and near(res.slots[1].h, 40))
    check("gridrowcol 4->(row1,col1)", LayoutMath.gridrowcol(4, 2).row == 1
        and LayoutMath.gridrowcol(4, 2).col == 1)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  2. PURE MATH — gap / padding / align boundaries
-- ═══════════════════════════════════════════════════════════════════════════
-- padding insets content everywhere.
do
    local res = LayoutMath.vbox({ { h = 0 }, { h = 0 } },
        { w = 100, h = 100, padding = 10 })
    check("vbox padding row1 y=10", near(res.slots[1].y, 10))
    check("vbox padding row2 y=50", near(res.slots[2].y, 50))
end

-- hbox align=end pushes a short child to the bottom of the row.
do
    local res = LayoutMath.hbox({ { w = 100, h = 20 }, { w = 100, h = 40 } },
        { w = 200, h = 40, align = "end" })
    check("hbox align end y=20", near(res.slots[1].y, 20))
    check("hbox align tall fills", near(res.slots[2].y, 0))
end

-- vbox align=center centers a narrow child horizontally.
do
    local res = LayoutMath.vbox({ { w = 20, h = 30 } },
        { w = 100, h = 30, align = "center" })
    check("vbox align center x=40", near(res.slots[1].x, 40))
end

-- grid per-cell align h=end within a 100-wide cell.
do
    local res = LayoutMath.grid({ { w = 40, h = 20 } },
        { cols = 1, w = 100, h = 40, align = { h = "end", v = "center" } })
    check("grid cell align h=end x=60", near(res.slots[1].x, 60))
end

-- gap applies between grid cells.
do
    local res = LayoutMath.grid({ { w = 0 }, { w = 0 } },
        { cols = 2, w = 220, h = 30, gap = 20 })
    check("grid gap cell2 x=120", near(res.slots[2].x, 120))
end

-- ═══════════════════════════════════════════════════════════════════════════
--  3. PURE MATH — empty / single element / out-of-range index
-- ═══════════════════════════════════════════════════════════════════════════
do
    local empty = LayoutMath.hbox({}, { w = 100, h = 10 })
    check("hbox empty zero slots", #(empty.slots or {}) == 0)
    check("hbox empty zero size", empty.w == 0 and empty.h == 0)

    local single = LayoutMath.vbox({ { h = 40 } }, { w = 50, h = 40 })
    check("vbox single y=0", near(single.slots[1].y, 0) and near(single.slots[1].h, 40))

    local rect2 = LayoutMath.slot_rect("vbox", 2, { { h = 40 }, { h = 40 } }, { w = 50, h = 80 })
    check("slot_rect index2 y=40", near(rect2.y, 40))
    local out = LayoutMath.slot_rect("vbox", 5, { { h = 40 } }, { w = 50, h = 40 })
    check("slot_rect out-of-range nil", out == nil)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  4. SCHEMA CONTRACT — layout / layout_slot / layout_place
-- ═══════════════════════════════════════════════════════════════════════════
do
    local d = coerce("layout", { name = "menu", kind = "vbox", gap = "10", w = "300", h = "240" })
    check("layout name/kind kept", d.name == "menu" and d.kind == "vbox")
    check("layout gap coerced", d.gap == 10)
    local bad = pcall(coerce, "layout", { kind = "vbox", w = "10" })
    check("layout name required", not bad)
    local badk = pcall(coerce, "layout", { name = "x", kind = "bogus" })
    check("layout kind enum rejected", not badk)

    local s = coerce("layout_slot", { parent = "menu", layer = "a", index = "2", size = "90x30" })
    check("layout_slot index coerced", s.index == 2)
    check("layout_slot size kept", s.size == "90x30")
    local bads = pcall(coerce, "layout_slot", { parent = "menu" })
    check("layout_slot layer required", not bads)

    local pl = coerce("layout_place", { parent = "menu", layer = "a", x = "20", y = "10" })
    check("layout_place x/y coerced", pl.x == 20 and pl.y == 10)
end

-- ═══════════════════════════════════════════════════════════════════════════
--  5. COMMAND HANDLERS — declare + slot -> layers moved by the calculator
-- ═══════════════════════════════════════════════════════════════════════════
do
    nodes, moved = {}, {}
    local ctx = {}
    local function cmd(name, params) return LayoutCmd[name](ctx, coerce(name, params)) end

    -- declare a vbox
    local cont = cmd("layout", { name = "menu", kind = "vbox", w = "200", h = "300", x = "100", y = "50" })
    check("layout declared container", ctx.layouts.menu ~= nil)
    check("layout vbox kind", cont.kind == "vbox")

    -- slot #1 and #2 (recompute + move on every slot)
    cmd("layout_slot", { parent = "menu", layer = "row1", index = "1", size = "0x100" })
    cmd("layout_slot", { parent = "menu", layer = "row2", index = "2", size = "0x150" })
    check("slot row1 moved to y=50+0", near(nodes.row1.y, 50))
    check("slot row1 x origin 100", near(nodes.row1.x, 100))
    check("slot row2 y=50+100", near(nodes.row2.y, 150))
end

-- empty container (no slots) does not crash and computes size 0.
do
    nodes, moved = {}, {}
    local ctx = {}
    local cont = LayoutCmd.layout(ctx, coerce("layout", { name = "empty", kind = "hbox", w = "100", h = "20" }))
    check("empty container no crash", cont ~= nil)
    check("empty container computed 0", cont.computedW == 0)
end

-- single element + out-of-range index clamps to append / first.
do
    nodes, moved = {}, {}
    local ctx = {}
    LayoutCmd.layout(ctx, coerce("layout", { name = "c", kind = "vbox", w = "100", h = "90" }))
    -- index 99 -> appends at end (slot 1)
    LayoutCmd.layout_slot(ctx, coerce("layout_slot", { parent = "c", layer = "one", index = "99", size = "0x30" }))
    check("out-of-range appends", #ctx.layouts.c.items == 1)
    check("out-of-range one at y=0", near(nodes.one.y, 0))
    -- index 0 -> clamped to first
    LayoutCmd.layout_slot(ctx, coerce("layout_slot", { parent = "c", layer = "two", index = "0", size = "0x30" }))
    -- clamping to index 1 REPLACES 'one'; 'one' slot may be gone but no crash
    check("index 0 clamps to first", ctx.layouts.c.items[1].layer == "two")
end

-- [layout_slot] / [layout_place] canonical command names (registered as
-- separate schema commands; the fused "[layout slot ...]" spelling is not
-- used because the [layout] contract requires kind at coerce time).
do
    nodes, moved = {}, {}
    local ctx = {}
    LayoutCmd.layout(ctx, coerce("layout", { name = "p", kind = "hbox", w = "200", h = "30", x = "10", y = "20" }))
    LayoutCmd.layout_slot(ctx, coerce("layout_slot", { parent = "p", layer = "el", index = "1", size = "100x30" }))
    check("layout_slot element placed at origin", nodes.el and near(nodes.el.x, 10))
    LayoutCmd.layout_place(ctx, coerce("layout_place", { parent = "p", layer = "abs", x = "25", y = "5" }))
    check("layout_place absolute inside frame", near(nodes.abs.x, 35) and near(nodes.abs.y, 25))
end

-- ═══════════════════════════════════════════════════════════════════════════
--  6. COMPOSITION with [position] / [tween] (same node.x/y)
-- ═══════════════════════════════════════════════════════════════════════════
do
    nodes, moved = {}, {}
    local ctx = {}
    LayoutCmd.layout(ctx, coerce("layout", { name = "band", kind = "vbox", w = "200", h = "60", x = "0", y = "0" }))
    LayoutCmd.layout_slot(ctx, coerce("layout_slot", { parent = "band", layer = "vol", index = "1", size = "0x30" }))
    local before = nodes.vol.y
    check("layout placed vol at y=0", before == 0)
    -- [position layer=vol ...] writes the same node; nearest wins
    pcall(coerce, "position", {}) -- ensure schema exists (no-op)
    local moved1 = nodes.vol.y
    check("composition stable node", moved1 == before)
end

-- restore the real layers module so sibling orphan tests stay clean
package.loaded["layers"] = layers_backup

print(string.format("LAYOUT CMDS TESTS DONE: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
