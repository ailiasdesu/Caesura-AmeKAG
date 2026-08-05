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

-- migrated production commands
check("pt migrated", Schema.isMigrated("pt"))
check("wait migrated", Schema.isMigrated("wait"))
check("scroll migrated", Schema.isMigrated("scroll"))
check("trans migrated", Schema.isMigrated("trans"))
check("move migrated", Schema.isMigrated("move"))
check("quake migrated", Schema.isMigrated("quake"))

-- text commands migrated
for _, c in ipairs({ "ch", "text", "ruby", "font" }) do
    check(c .. " migrated", Schema.isMigrated(c))
end
p = Schema.coerce("ch", { max_width = "99999" }, {})
check("ch max_width clamped", p.max_width == 4096)
p = Schema.coerce("ruby", { ruby_scale = "9" }, {})
check("ruby scale clamped", p.ruby_scale == 2.0)
p = Schema.coerce("font", { size = "1" }, {})
check("font size clamped to min", p.size == 4)
ok, err = pcall(function() Schema.coerce("font", { size = "abc" }, {}) end)
check("font bad size throws", not ok)

-- audio commands migrated
for _, c in ipairs({ "playbgm", "playse", "stopbgm", "stopse", "fadebgm", "fadevol" }) do
    check(c .. " migrated", Schema.isMigrated(c))
end
-- playbgm volume clamp + required file
local ok, err = pcall(function()
    Schema.coerce("playbgm", { volume = "9" }, {})
end)
check("playbgm missing file throws", not ok)
p = Schema.coerce("playbgm", { file = "x.wav", volume = "9" }, {})
check("playbgm volume clamped", p.volume == 1.5)
p = Schema.coerce("playbgm", { file = "x.wav" }, {})
check("playbgm defaults", p.volume == 1.0 and p.loop == true)

-- scroll contract behavior (typed + clamped)
p = Schema.coerce("scroll", { speed = "5000" }, {})
check("scroll speed clamped to 1000", p.speed == 1000)
p = Schema.coerce("scroll", { size = "0" }, {})
check("scroll size clamped to 8", p.size == 8)
p = Schema.coerce("scroll", {}, {})
check("scroll defaults", p.speed == 60 and p.size == 28 and p.text == "")

if failed and failed > 0 then os.exit(1) end
print("SCHEMA TESTS DONE")
