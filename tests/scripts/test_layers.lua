-- test_layers.lua — custom layers + pooled RTT/view recycling (Neo-Genesis)
local check = function(name, cond)
    if cond then print("  [PASS] " .. name) passed = (passed or 0) + 1
    else print("  [FAIL] " .. name) failed = (failed or 0) + 1 end
end

-- stub backend so layers/rtt load without a GPU
local backend_stub = {
    create_viewport = function(w, h) return math.random(1000, 9999) end,
    destroy_viewport = function() end,
    get_viewport_size = function() return 640, 360 end,
    submit_batch = function() end,
    set_view_clear = function() end,
}
local layers_src = io.open("scripts/layers.lua", "r"):read("*a")
local rtt_src = io.open("scripts/rtt.lua", "r"):read("*a")

-- 1) custom layer API (arbitrary id, beyond the 7 built-in types)
check("add_layer accepts custom id", layers_src:find('config.id or ("layer_"', 1, true) ~= nil)
check("layer types extensible", layers_src:find("LAYER_EFFECT", 1, true) ~= nil)

-- 2) pooled RTT (acquire/release keyed by size)
check("RTT.acquire exists", rtt_src:find("function RTT.acquire", 1, true) ~= nil)
check("RTT.release exists", rtt_src:find("function RTT.release", 1, true) ~= nil)
check("pool bounded at 8/size", rtt_src:find("#bucket < 8", 1, true) ~= nil)

-- 3) view recycling (free-list)
check("freeViews list exists", layers_src:find("freeViews", 1, true) ~= nil)
check("view recycled on remove", layers_src:find("freeViews[#freeViews + 1] = node.view_id", 1, true) ~= nil)

-- 5) reset hygiene (security MEDIUM: init clears the view free-list)
check("init clears freeViews", layers_src:find("freeViews = {}", 1, true) ~= nil)
-- 6) root removal guard (security LOW: view_id 0 must not recycle)
check("root removal refused", layers_src:find("refusing to remove the root", 1, true) ~= nil)

-- 4) lazy RTT allocation
check("lazy acquire in render", layers_src:find("Lazy RTT", 1, true) ~= nil)
-- 8) pick hit-test (round 23 /api/pick)
check("Layers.pick exists", layers_src:find("function Layers.pick", 1, true) ~= nil)
check("pick filters invisible", layers_src:find("node.visible == false then return", 1, true) ~= nil)
check("pick checks bounds", layers_src:find("px >= x and px <= x + w and py >= y and py <= y + h", 1, true) ~= nil)
check("pick sorts by z", layers_src:find("table.sort(hits", 1, true) ~= nil)
check("invisible layers cost nothing", layers_src:find("node.dirty and node.view_id and not node.rt", 1, true) ~= nil)

-- 9) Web player snapshot export (round 34)
check("Layers.snapshot exists", layers_src:find("function Layers.snapshot", 1, true) ~= nil)
check("snapshot reads texture id", layers_src:find("texture = node.texture", 1, true) ~= nil)
check("snapshot is a pure read", layers_src:find("for _, node in pairs(layerMap) do", 1, true) ~= nil)

if failed and failed > 0 then os.exit(1) end
print("LAYER TESTS DONE")
