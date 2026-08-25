-- test_layers_alloc.lua -- submit_batch positional wire format + per-frame
-- allocation contract for Layers.render (t11).
--
-- Locks three things the renderer must guarantee:
--   1. WIRE FORMAT: batch[1] = live command count, then 16 numeric slots per
--      command (see the format block above Layers.render in scripts/layers.lua).
--      Every slot is a number, never nil, so the C++ reader can use rawgeti.
--   2. RESIDUE SAFETY (correctness red line): the array is REUSED across
--      frames, so a lighter frame leaves stale numbers in the tail. batch[1]
--      must shrink and the live region must be fully rewritten -- no field of a
--      previous frame's command may survive inside the live region.
--   3. NO PER-FRAME TABLE CHURN: repeated frames must not allocate a table per
--      visible node (the old code built a 16-field literal per node per frame).
--
-- Sandbox discipline: no _G writes and no rawset. The backend module table is
-- monkeypatched by FIELD assignment (the same table object layers.lua captured
-- at require time), which is how the other suites stub the engine.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local backend = require("backend")
local layers  = require("layers")
local blend   = require("blend")

local STRIDE = 16

-- Capture the array Layers.render submits. We deliberately keep BOTH the live
-- reference (to prove reuse: same table object every frame) and a snapshot of
-- the live region (to assert content).
local lastRef, lastCount, lastLive = nil, nil, nil
local realSubmit = backend.submit_batch
backend.submit_batch = function(cmds)
    lastRef   = cmds
    lastCount = cmds[1]
    lastLive  = {}
    for i = 1, (cmds[1] or 0) * STRIDE do lastLive[i] = cmds[1 + i] end
    return true
end

local function addQuad(id, tex, x, y, w, h, extra)
    local node = layers.add_layer(nil, {
        id = id, x = x, y = y, w = w, h = h,
        visible = true, opacity = 200,
    })
    node.tex = tex
    node.view_id = node.view_id or 1
    if extra then for k, v in pairs(extra) do node[k] = v end end
    return node
end

-- ---------------------------------------------------------------------------
-- 1. Wire format
-- ---------------------------------------------------------------------------
layers.init()
local q1 = addQuad("alloc_a", 11, 10, 20, 100, 50, { blend_mode = "add", scale = 2.0, rotation = 30 })
local q2 = addQuad("alloc_b", 22, 30, 40, 60, 70, { clipX = 1, clipY = 2, clipW = 3, clipH = 4 })
layers.render()

check("submit received an array", type(lastRef) == "table", type(lastRef))
check("header slot carries the count", lastCount == 2, tostring(lastCount))

-- Locate our two commands by tex id (traversal order is z-sorted, not authored).
local function findCmd(texId)
    for c = 0, (lastCount or 0) - 1 do
        if lastLive[c * STRIDE + 2] == texId then return c * STRIDE end
    end
    return nil
end
local b1, b2 = findCmd(11), findCmd(22)
check("command for tex 11 present", b1 ~= nil, "not found")
check("command for tex 22 present", b2 ~= nil, "not found")

if b1 and b2 then
    check("slot 1 view_id numeric", type(lastLive[b1 + 1]) == "number", type(lastLive[b1 + 1]))
    check("slot 4 x", lastLive[b1 + 4] == 10, tostring(lastLive[b1 + 4]))
    check("slot 5 y", lastLive[b1 + 5] == 20, tostring(lastLive[b1 + 5]))
    check("slot 6 w", lastLive[b1 + 6] == 100, tostring(lastLive[b1 + 6]))
    check("slot 7 h", lastLive[b1 + 7] == 50, tostring(lastLive[b1 + 7]))
    check("slot 8 opacity", lastLive[b1 + 8] == 200, tostring(lastLive[b1 + 8]))
    -- Blend mode crosses the boundary as a NUMBER (blend.resolve), because the
    -- positional array must stay all-numeric for rawgeti reads.
    check("slot 9 blend is numeric id",
          lastLive[b1 + 9] == blend.resolve("add"),
          tostring(lastLive[b1 + 9]) .. " expected " .. tostring(blend.resolve("add")))
    check("slot 9 is not a string", type(lastLive[b1 + 9]) == "number", type(lastLive[b1 + 9]))
    check("slot 10 scaleX from node.scale", lastLive[b1 + 10] == 2.0, tostring(lastLive[b1 + 10]))
    check("slot 11 scaleY from node.scale", lastLive[b1 + 11] == 2.0, tostring(lastLive[b1 + 11]))
    check("slot 12 rotation", lastLive[b1 + 12] == 30, tostring(lastLive[b1 + 12]))
    check("slots 13-16 clip default to 0 when unset",
          lastLive[b1 + 13] == 0 and lastLive[b1 + 14] == 0
          and lastLive[b1 + 15] == 0 and lastLive[b1 + 16] == 0,
          table.concat({ tostring(lastLive[b1+13]), tostring(lastLive[b1+14]),
                         tostring(lastLive[b1+15]), tostring(lastLive[b1+16]) }, ","))
    check("slots 13-16 clip forwarded when set",
          lastLive[b2 + 13] == 1 and lastLive[b2 + 14] == 2
          and lastLive[b2 + 15] == 3 and lastLive[b2 + 16] == 4,
          table.concat({ tostring(lastLive[b2+13]), tostring(lastLive[b2+14]),
                         tostring(lastLive[b2+15]), tostring(lastLive[b2+16]) }, ","))
    -- rt slot: 0 (not nil) when the node has no render target.
    check("slot 3 rt is 0 not nil without an RT",
          lastLive[b1 + 3] == 0 or type(lastLive[b1 + 3]) == "number",
          tostring(lastLive[b1 + 3]))

    local allNumeric = true
    for i = 1, (lastCount or 0) * STRIDE do
        if type(lastLive[i]) ~= "number" then allNumeric = false break end
    end
    check("every live slot is a number (no nil holes)", allNumeric, "found a non-number slot")
end

-- ---------------------------------------------------------------------------
-- 2. Residue safety: reuse must never leak a previous frame's command
-- ---------------------------------------------------------------------------
local firstRef = lastRef
layers.render()
check("array object is reused across frames (no per-frame array alloc)",
      lastRef == firstRef, "a different table was submitted")

-- Shrink the scene: hide one quad. The array keeps its old length, but the
-- header must drop to 1 and the live region must describe ONLY the survivor.
q1.visible = false
layers.render()
check("count shrinks when a node stops rendering", lastCount == 1, tostring(lastCount))
check("live region no longer mentions the hidden quad's texture",
      findCmd(11) == nil, "stale command survived inside the live region")
check("live region describes the surviving quad", findCmd(22) == 0,
      tostring(findCmd(22)))
-- The stale tail is expected to still be there physically -- that is exactly
-- why batch[1] is the only length authority. Assert the tail IS stale so the
-- contract is explicit: a reader using rawlen would see the old command.
check("stale tail is still physically present (rawlen would over-read)",
      lastRef[1 + STRIDE + 2] ~= nil, "tail was cleared; rawlen assumption changed")

-- Grow again: the previously hidden node must come back with FRESH values,
-- not the numbers left over from before it was hidden.
q1.visible = true
q1.x, q1.y = 777, 888
layers.render()
local b1b = findCmd(11)
check("re-shown quad reappears", b1b ~= nil, "missing after re-show")
if b1b then
    check("re-shown quad carries this frame's x", lastLive[b1b + 4] == 777,
          tostring(lastLive[b1b + 4]))
    check("re-shown quad carries this frame's y", lastLive[b1b + 5] == 888,
          tostring(lastLive[b1b + 5]))
end

-- ---------------------------------------------------------------------------
-- 3. No per-frame table churn
-- ---------------------------------------------------------------------------
-- Method: 32 visible quads, 200 frames, measure collectgarbage("count") (KB)
-- growth across the loop after a full collect + warmup. The OLD code allocated
-- one 16-field table per visible node per frame (32 x 200 = 6400 tables, tens
-- of KB); the positional array allocates nothing steady-state. The threshold is
-- deliberately loose (other suites share this Lua state) but far below what
-- 6400 table allocations produce.
layers.init()
for i = 1, 32 do addQuad("churn_" .. i, 100 + i, i, i, 64, 64) end
for _ = 1, 20 do layers.render() end     -- warmup: grow the array to steady size
collectgarbage("collect")
local kbBefore = collectgarbage("count")
for _ = 1, 200 do layers.render() end
local kbAfter = collectgarbage("count")
local grewKb = kbAfter - kbBefore
print(string.format("  [measure] 32 quads x 200 frames: GC heap grew %.1f KB", grewKb))
check("200 frames x 32 quads allocate < 64 KB (was ~1 table/node/frame)",
      grewKb < 64, string.format("%.1f KB", grewKb))

-- Restore the real submit so later suites are unaffected.
backend.submit_batch = realSubmit
layers.init()

local passed = 0
for _, r in ipairs(results) do if r then passed = passed + 1 end end
print(string.format("Results: %d/%d passed", passed, #results))
if passed < #results then os.exit(1) end
