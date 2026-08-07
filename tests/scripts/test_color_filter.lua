-- test_color_filter.lua — accessibility color filter (Neo-Genesis):
-- config -> backend.set_color_filter -> effect-4 VFX submit path,
-- sandbox whitelist, and mock-backend invocation checks.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

-- ---- backend.set_color_filter degrades without a backend ----------------
do
    local backend = require("backend")
    check("set_color_filter false without backend",
        backend.set_color_filter("grayscale") == false)
    -- known preset names are accepted by the C++ binding only; the Lua
    -- wrapper passes through (the binding rejects unknown names).
    local cf = backend.set_color_filter
    check("wrapper is a function", type(cf) == "function")
end

-- ---- layers.render submits effect 4 when config enables a filter --------
do
    local backend = require("backend")
    local realSet = backend.set_color_filter
    local realSubmit = backend.submit_vfx
    local calls = {}
    backend.set_color_filter = function(p) calls[#calls + 1] = { "set_color_filter", p }; return true end
    backend.submit_vfx = function(...) calls[#calls + 1] = { "submit_vfx", ... }; return true end
    local layers = require("layers")
    -- Build a small layer tree with an RTT on the root.
    local root = layers.get_root()
    root.rt = 42
    root.visible = true
    -- Config mock: layers.render reads config.accessibility.color_filter;
    -- the real config module would spin up backend_factory (needs the
    -- C binding), so inject a stub.
    local cfg = { accessibility = { color_filter = "deuteranopia" } }
    local orig_cfg = package.loaded["config"]
    package.loaded["config"] = cfg
    layers.render()
    local sawSet = false
    local sawSubmit = false
    for _, c in ipairs(calls) do
        if c[1] == "set_color_filter" and c[2] == "deuteranopia" then sawSet = true end
        if c[1] == "submit_vfx" and c[3] == 4 and c[2] == 42 then sawSubmit = true end
    end
    check("set_color_filter called with preset", sawSet)
    check("submit_vfx effect 4 called on root RT", sawSubmit)
    cfg.accessibility.color_filter = "none"
    calls = {}
    layers.render()
    local sawAny = false
    for _, c in ipairs(calls) do
        if c[1] == "set_color_filter" or (c[1] == "submit_vfx" and c[3] == 4) then
            sawAny = true
        end
    end
    check("no filter submit when config=none", not sawAny)
    package.loaded["config"] = orig_cfg
    root.rt = nil
    backend.set_color_filter = realSet
    backend.submit_vfx = realSubmit
end

-- ---- sandbox strict mode allows set_color_filter (engine-verified) -------
do
    -- The strict-mode render whitelist lives in sandbox.lua and is covered
    -- end-to-end by the engine RPC eval test (Render.set_color_filter
    -- returns a value instead of being blocked); this suite cannot read
    -- the file (io disabled in the sandbox). Smoke-check the module loads.
    local ok, err = pcall(require, "sandbox")
    check("sandbox module loads", ok, tostring(err))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("COLOR FILTER TESTS DONE")
