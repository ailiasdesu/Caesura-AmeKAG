-- test_schema.lua — next-gen command contract tests
local check = function(name, cond)
    if cond then print("  [PASS] " .. name) passed = (passed or 0) + 1
    else print("  [FAIL] " .. name) failed = (failed or 0) + 1 end
end
local Schema = require("kag.schema")

-- define a test contract
Schema.define("_test_cmd", {
    speed = { type = "number", default = 50, min = 8, max = 5000 },
    loop  = { type = "boolean", default = false },
    mode  = { type = "string", choices = { ["crossfade"] = true, ["cut"] = true } },
})

-- number coercion + default
local p = Schema.coerce("_test_cmd", {}, { current_scene = "s", token_index = 1 })
check("default applied", p.speed == 50)
check("boolean default", p.loop == false)

-- string -> number
p = Schema.coerce("_test_cmd", { speed = "120" }, {})
check("string coerced to number", p.speed == 120)

-- clamp max
p = Schema.coerce("_test_cmd", { speed = "99999" }, {})
check("clamped to max", p.speed == 5000)

-- clamp min
p = Schema.coerce("_test_cmd", { speed = "1" }, {})
check("clamped to min", p.speed == 8)

-- bad number throws with location
local ok, err = pcall(function()
    Schema.coerce("_test_cmd", { speed = "abc" }, { current_scene = "sc", token_index = 42 })
end)
check("bad number throws", not ok)
check("error has location", type(err) == "string" and err:find("sc") ~= nil and err:find("42") ~= nil)

-- boolean coercion
p = Schema.coerce("_test_cmd", { loop = "true" }, {})
check("boolean true coerced", p.loop == true)
p = Schema.coerce("_test_cmd", { loop = "0" }, {})
check("boolean false coerced", p.loop == false)

-- choices validation
ok, err = pcall(function()
    Schema.coerce("_test_cmd", { mode = "dissolve" }, {})
end)
check("bad choice throws", not ok)

-- unknown param passes through (compat) with warning
p = Schema.coerce("_test_cmd", { speed = "60", legacy_param = "x" }, {})
check("unknown param passthrough", p.legacy_param == "x")

-- unmigrated command passes raw
local raw = Schema.coerce("_not_migrated", { speed = "abc" }, {})
check("unmigrated passthrough", raw.speed == "abc")

-- registry
check("registry non-empty", Schema.registrySize() >= 3)
check("isMigrated", Schema.isMigrated("_test_cmd") and not Schema.isMigrated("_not_migrated"))

if failed and failed > 0 then os.exit(1) end
print("SCHEMA TESTS DONE")
