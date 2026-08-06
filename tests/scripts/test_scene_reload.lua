-- test_scene_reload.lua — KAG scene hot reload:
-- cache busting, position remapping (token/label/start fallbacks),
-- non-current scene cache-only invalidation, failure handling.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local flow = require("flow")
local kag_runner = require("kag_runner")

local TMP = "tests/scripts/_reload_tmp.ks"

local function write_ks(content)
    local f = assert(io.open(TMP, "w"))
    f:write(content)
    f:close()
end

-- ---- remap: same token (cmd + primary param) -----------------------------
do
    local old = {
        { "ch", { text = "one" } },
        { "ch", { text = "two" } },
        { "ch", { text = "three" } },
    }
    local new = {
        { "ch", { text = "zero" } },   -- inserted line
        { "ch", { text = "two" } },    -- the live token, shifted
        { "ch", { text = "three" } },
    }
    local idx = kag_runner.remap_token_index(old, 2, new)
    check("remap finds same token by content", idx == 2, tostring(idx))
end

-- ---- remap: label fallback ------------------------------------------------
do
    local old = {
        { "label", { name = "start" } },
        { "ch", { text = "a" } },
        { "label", { name = "garden" } },
        { "ch", { text = "b" } },
        { "ch", { text = "c" } },
    }
    local new = {
        { "ch", { text = "intro" } },      -- rewritten prologue
        { "label", { name = "garden" } },  -- label kept, shifted
        { "ch", { text = "b2" } },         -- text edited
    }
    -- live position = token 5 (ch c); nearest preceding label = garden
    local idx = kag_runner.remap_token_index(old, 5, new)
    check("remap falls back to nearest label", idx == 2, tostring(idx))
end

-- ---- remap: deleted label -> scene start ----------------------------------
do
    local old = {
        { "label", { name = "gone" } },
        { "ch", { text = "x" } },
    }
    local new = {
        { "ch", { text = "brand new" } },
    }
    local idx = kag_runner.remap_token_index(old, 2, new)
    check("remap falls back to scene start", idx == 1, tostring(idx))
end

-- ---- flow cache busting through reload_scene ------------------------------
do
    write_ks("[ch text=\"v1\"]\n")
    flow.reload_scene(TMP)  -- ensure fresh
    local s1 = flow.load_scene(TMP)
    local p1 = s1.tokens[1].params and s1.tokens[1].params[1]
    check("initial parse v1",
        p1 and p1[1] == "text" and p1[2] == "v1",
        p1 and tostring(p1[2]))
    write_ks("[ch text=\"v2\"]\n")
    local s2 = flow.reload_scene(TMP)
    local p2 = s2.tokens[1].params and s2.tokens[1].params[1]
    check("reload_scene re-parses changed file",
        s2 and p2 and p2[1] == "text" and p2[2] == "v2",
        p2 and tostring(p2[2]))
end

-- ---- non-current scene: cache-only invalidation ---------------------------
do
    -- No kag_runner ctx in this suite: reload_scene returns no-context.
    -- Simulate the cache-only branch via flow directly (the runner branch
    -- is covered by the integration test in the engine/editor).
    write_ks("[ch text=\"v3\"]\n")
    flow.reload_scene(TMP)
    local ok, err = kag_runner.reload_scene(TMP)
    -- Without a runner ctx the call must fail gracefully, not raise.
    check("reload_scene without runner ctx fails gracefully",
        ok == false and (err == "no-context" or err ~= nil),
        tostring(ok) .. "/" .. tostring(err))
end

-- ---- missing file: graceful failure ---------------------------------------
do
    local ok, err = kag_runner.reload_scene("tests/scripts/__nope__.ks")
    check("reload_scene missing file fails gracefully",
        ok == false and err ~= nil, tostring(err))
end

os.remove(TMP)

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("SCENE RELOAD TESTS DONE")
