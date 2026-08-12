-- test_schema_types.lua — Battle 2b: contract type-system deepening.
-- list / enum / file types in schema.coerce, plus the file-type upgrade
-- of audio/layer storage params (cross-validated asset paths).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

local s = require("kag.schema")
-- load command modules so their contracts register
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.text")
pcall(require, "kag")  -- [play] bus contract lives in kag.lua

-- ---------------------------------------------------------------------------
-- 1. list type: comma-separated -> array, typed elements
-- ---------------------------------------------------------------------------
s.define("_t_list", {
    colors = { type = "list", item_type = "string" },
    nums = { type = "list", item_type = "number" },
    flags = { type = "list", item_type = "boolean" },
})
local p1 = s.coerce("_t_list", { colors = "red, green, blue" }, {})
check("list string split+trim",
      p1.colors[1] == "red" and p1.colors[2] == "green" and p1.colors[3] == "blue")
local p2 = s.coerce("_t_list", { nums = "1,2,3" }, {})
check("list number elements", p2.nums[1] == 1 and p2.nums[3] == 3)
check("list number arithmetic", p2.nums[1] + p2.nums[2] + p2.nums[3] == 6)
local p3 = s.coerce("_t_list", { flags = "true,0,yes" }, {})
check("list boolean elements",
      p3.flags[1] == true and p3.flags[2] == false and p3.flags[3] == true)
local okBad = pcall(s.coerce, "_t_list", { nums = "1,abc" }, {})
check("list bad number element rejected", not okBad)
local p4 = s.coerce("_t_list", { colors = { "x", "y" } }, {})
check("list table passthrough", p4.colors[2] == "y")

-- ---------------------------------------------------------------------------
-- 2. enum type: values validation
-- ---------------------------------------------------------------------------
s.define("_t_enum", { mode = { type = "enum", values = { "on", "off", "toggle" } } })
local e1 = s.coerce("_t_enum", { mode = "toggle" }, {})
check("enum valid value", e1.mode == "toggle")
local okE = pcall(s.coerce, "_t_enum", { mode = "invalid" }, {})
check("enum invalid value rejected", not okE)
-- legacy choices map still works as enum source
s.define("_t_enum2", { bus = { type = "enum", choices = { bgm = true, se = true } } })
local e2 = s.coerce("_t_enum2", { bus = "se" }, {})
check("enum via choices map", e2.bus == "se")
local okE2 = pcall(s.coerce, "_t_enum2", { bus = "voice" }, {})
check("enum choices invalid rejected", not okE2)

-- ---------------------------------------------------------------------------
-- 3. file type: traversal/empty rejection, resolver cross-validation
-- ---------------------------------------------------------------------------
s.define("_t_file", { storage = { type = "file" } })
local f1 = pcall(s.coerce, "_t_file", { storage = "../evil.png" }, {})
check("file traversal rejected (no ctx)", not f1)
local f2 = pcall(s.coerce, "_t_file", { storage = "/etc/passwd" }, {})
check("file absolute rejected", not f2)
local f3, pF = pcall(s.coerce, "_t_file", { storage = "assets/bg/room.png" }, {})
check("file valid path ok", f3 and pF.storage == "assets/bg/room.png")
local f4 = pcall(s.coerce, "_t_file", { storage = "assets/bg/missing.png" },
                 { resolve_file = function() return nil end })
check("file missing via resolver rejected", not f4)
local f5, pF5 = pcall(s.coerce, "_t_file", { storage = "assets/bg/x.png" },
                     { resolve_file = function() return "found" end })
check("file exists via resolver ok", f5 and pF5.storage == "assets/bg/x.png")

-- ---------------------------------------------------------------------------
-- 4. production contracts upgraded: audio/layer storage is file-typed
-- ---------------------------------------------------------------------------
local contracts = s.dumpContracts()
local function paramType(cmd, pname)
    local specs = contracts[cmd]
    return specs and specs[pname] and specs[pname].type
end
check("playbgm file is file-typed", paramType("playbgm", "file") == "file")
check("playbgm storage is file-typed", paramType("playbgm", "storage") == "file")
check("playse file is file-typed", paramType("playse", "file") == "file")
check("bg storage is file-typed", paramType("bg", "storage") == "file")
check("fg storage is file-typed", paramType("fg", "storage") == "file")
check("image storage is file-typed", paramType("image", "storage") == "file")
check("xfadebgm storage is file-typed", paramType("xfadebgm", "storage") == "file")

-- runtime coercion of an upgraded command rejects traversal
local okT = pcall(s.coerce, "bg", { storage = "../../evil.png" }, {})
check("bg traversal rejected by file type", not okT)

-- ---------------------------------------------------------------------------
-- 5. enum on the production [play] bus contract (already choices)
-- ---------------------------------------------------------------------------
local okBus = pcall(s.coerce, "play", { bus = "invalid_bus" }, {})
check("play bus choices still enforced", not okBus)

-- Exit gate.
if failed > 0 then
    print(string.format("SCHEMA TYPES TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("SCHEMA TYPES TESTS DONE (%d passed)", passed))
