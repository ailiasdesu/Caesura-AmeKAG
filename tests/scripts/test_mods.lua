-- test_mods.lua — mod loader: scene/resource override, priority order,
-- enable/disable fallback, integration with flow.load_scene.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local mods = require("mods")

-- Fixture: create a fake mod tree (and clean up afterwards).
local function write(path, content)
    local dir = path:match("^(.*)/[^/]+$")
    if dir then
        os.execute('mkdir "' .. dir:gsub("/", "\\") .. '" 2>nul')
    end
    local f = assert(io.open(path, "w"))
    f:write(content)
    f:close()
end

write("mods/testmod_a/assets/script/main.ks", "[ch text=\"mod A scene\"]\n")
write("mods/testmod_a/assets/bgm/remix.ogg", "fake-a")
write("mods/testmod_b/assets/script/main.ks", "[ch text=\"mod B scene\"]\n")

-- ---- registration / priority ----------------------------------------------
mods.register("testmod_a", 10)
mods.register("testmod_b", 5)
check("register stores priority",
    mods.get("testmod_a").priority == 10
        and mods.get("testmod_b").priority == 5)

-- ---- resolution without enable: base path --------------------------------
check("disabled mod does not override",
    mods.resolve("assets/script/main.ks") == "assets/script/main.ks")

-- ---- enable + priority order ----------------------------------------------
mods.enable("testmod_a")
mods.enable("testmod_b")
check("enable + resolve picks mod A (higher priority)",
    mods.resolve("assets/script/main.ks") == "mods/testmod_a/assets/script/main.ks")
check("resolve missing asset falls back to base",
    mods.resolve("assets/bgm/never.ogg") == "assets/bgm/never.ogg")
check("resolve asset present only in B",
    mods.resolve("assets/bgm/remix.ogg") == "mods/testmod_a/assets/bgm/remix.ogg")

-- ---- list order (highest priority first) ----------------------------------
local ordered = mods.list()
check("list order by priority desc",
    ordered[1] == "testmod_a" and ordered[2] == "testmod_b",
    table.concat(ordered, ","))

-- ---- disable fallback ------------------------------------------------------
mods.disable("testmod_a")
check("disable A falls back to B",
    mods.resolve("assets/script/main.ks") == "mods/testmod_b/assets/script/main.ks")
mods.disable("testmod_b")
check("disable all falls back to base",
    mods.resolve("assets/script/main.ks") == "assets/script/main.ks")
mods.enable("testmod_a")
mods.enable("testmod_b")

-- ---- flow.load_scene integration ------------------------------------------
do
    local flow = require("flow")
    -- base scene exists in the repo; the mod overrides it
    local scene = flow.load_scene("assets/script/main.ks")
    check("flow.load_scene resolves mod override",
        scene and scene.path == "mods/testmod_a/assets/script/main.ks"
            and scene.base_path == "assets/script/main.ks",
        scene and tostring(scene.path))
    local first = scene.tokens[1]
    -- parse_file returns raw LPeg captures: {type="command", cmd=...,
    -- params={{key,value},...}} -- the text value sits at params[1][2].
    local p1 = first and first.params and first.params[1]
    check("mod scene tokens loaded",
        first and first.cmd == "ch" and p1 and p1[1] == "text"
            and p1[2] == "mod A scene",
        first and tostring(first.cmd)
            .. "/" .. tostring(p1 and p1[2]))
    -- cache is keyed by resolved path: disabling A and reloading must
    -- NOT return the cached mod-A scene
    mods.disable("testmod_a")
    local scene2 = flow.load_scene("assets/script/main.ks")
    check("cache keyed by resolved path",
        scene2 and scene2.path == "mods/testmod_b/assets/script/main.ks",
        scene2 and tostring(scene2.path))
end

-- ---- invalid mod names rejected -------------------------------------------
do
    local ok1 = pcall(mods.register, "../evil", 1)
    local ok2 = pcall(mods.register, "a/b", 1)
    check("path traversal mod name rejected",
        not ok1 and not ok2, tostring(ok1) .. tostring(ok2))
end

-- ---- cleanup ---------------------------------------------------------------
os.execute('rmdir /s /q mods\\testmod_a 2>nul')
os.execute('rmdir /s /q mods\\testmod_b 2>nul')
mods.disable("testmod_a")
mods.disable("testmod_b")

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MODS TESTS DONE")
