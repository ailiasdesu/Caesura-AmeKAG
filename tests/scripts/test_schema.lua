-- test_schema.lua — next-gen command contract tests
local check = function(name, cond)
    if cond then print("  [PASS] " .. name) passed = (passed or 0) + 1
    else print("  [FAIL] " .. name) failed = (failed or 0) + 1 end
end
local Schema = require("kag.schema")
-- Load every command module so their contracts register even when this
-- test runs standalone (the suite order otherwise masks missing requires).
pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.transition")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.video")
pcall(require, "kag.commands.save")

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

-- interpolation ($f.var in interpolate=true params)
do
    local ctx = { f = { name = "Ame", hp = 42 }, sf = { score = 7 } }
    local ok2, err2 = pcall(function()
        Schema.coerce("ch", { name = "Ame", text = "Hi $f.name (hp $f.hp)", voice = "v.ogg", sprite = "s.png" }, ctx)
    end)
    if ok2 then
        -- ch contract interpolates text; capture via a local redefine
        -- (simplest: coerce directly with the ch contract from text.lua)
        -- fall back to checking the mechanism via a local contract:
        Schema.define("_interp_test", { text = { type = "string", default = "", interpolate = true } })
        local p2 = Schema.coerce("_interp_test", { text = "Hi $f.name ($sf.score)" }, ctx)
        check("interpolation expands f/sf vars", p2.text == "Hi Ame (7)")
        local p3 = Schema.coerce("_interp_test", { text = "unknown $f.nope stays" }, ctx)
        check("unresolved var left as-is", p3.text == "unknown $f.nope stays")
        local p4 = Schema.coerce("_interp_test", { text = "no vars" }, {})
        check("plain text unchanged", p4.text == "no vars")
    else
        check("ch coerce ok", false)
    end
end

-- real ch contract interpolates (review nit: assert the actual contract)
do
    local ctx = { f = { name = "Ame" } }
    local p2 = Schema.coerce("ch", { name = "A", text = "Hi $f.name", voice = "v.ogg", sprite = "s.png" }, ctx)
    check("ch text interpolates", p2.text == "Hi Ame")
end
-- scheduler routes bare text through the ch contract (should-fix)
do
    local src = io.open("scripts/scheduler.lua", "r")
    local sched = src and src:read("*a") or ""
    if src then src:close() end
    check("bare text coerced as ch", sched:find('schema.coerce("ch", params, ctx)', 1, true) ~= nil)
end

-- ${expr} full-expression interpolation
do
    local ctx = { f = { hp = 42, name = "Ame" }, sf = { score = 7 } }
    local p2 = Schema.coerce("_interp_test",
        { text = "hp ${f.hp*2} n ${f.name} s ${sf.score+1}" }, ctx)
    check("expr interpolation evaluates", p2.text == "hp 84 n Ame s 8")
    local p3 = Schema.coerce("_interp_test", { text = "bad ${1+}" }, ctx)
    check("bad expr stays literal", p3.text == "bad ${1+}")
    local p4 = Schema.coerce("_interp_test", { text = "mixed $f.name ${f.hp}" }, ctx)
    check("var + expr mixed", p4.text == "mixed Ame 42")
end

-- volume setter contracts (clamp regression class)
do
    pcall(require, "kag.commands.audio")
    for _, c in ipairs({ "setbgmvolume", "setsevolume", "setvoicevolume", "playbgmstop" }) do
        check(c .. " migrated", Schema.isMigrated(c))
    end
    local pv = Schema.coerce("setbgmvolume", { volume = "9" }, {})
    check("setbgmvolume clamped", pv.volume == 1.5)
    pv = Schema.coerce("setvoicevolume", { volume = "-1" }, {})
    check("setvoicevolume min clamped", pv.volume == 0)
    pv = Schema.coerce("playbgmstop", { fadeout = "99999" }, {})
    check("playbgmstop fade clamped", pv.fadeout == 30000)
    pv = Schema.coerce("playbgmstop", { volume = "9" }, {})
    check("playbgmstop volume clamped", pv.volume == 1.5)
end

-- storage forwarding through play (contract-advertised)
do
    local audio = require("kag.commands.audio")
    local orig = audio.playbgm
    local got
    audio.playbgm = function(_, p) got = p end
    local kag2 = require("kag")
    if kag2 and kag2.play then
        pcall(function() kag2.play({}, { bus = "bgm", storage = "x.ogg", volume = "1" }) end)
    end
    audio.playbgm = orig  -- restore even if play raised
    check("play forwards storage to playbgm", got and got.storage == "x.ogg")
end

-- [bgm] command-name contract (security: pre-dispatch clamp)
do
    pcall(require, "kag")
    check("bgm migrated", Schema.isMigrated("bgm"))
    local pb = Schema.coerce("bgm", { file = "x.wav", volume = "9" }, {})
    check("bgm volume clamped pre-dispatch", pb.volume == 1.5)
end

-- [nameplate]/[textbox] contracts (message-window modernization)
do
    pcall(require, "kag.commands.text")
    check("nameplate migrated", Schema.isMigrated("nameplate"))
    check("textbox migrated", Schema.isMigrated("textbox"))
    local pn = Schema.coerce("nameplate", {}, {})
    check("nameplate defaults", pn.x == 32 and pn.w == 220 and pn.opacity == 220)
    local pn2 = Schema.coerce("nameplate", { size = "99" }, {})
    check("nameplate size passthrough (no contract)", pn2.size == "99")
    local doc = io.open("docs/api/command-contracts.md", "r")
    local docTxt = doc and doc:read("*a") or ""
    if doc then doc:close() end
    -- strict check: the nameplate section (between headers) must exist
    -- and must not contain a | `size` | row (dead knob verified absent).
    local secEnd = docTxt:find("### `%[nameplate%]`")
    check("nameplate docs section found", secEnd ~= nil)
    if secEnd then
        local _, nextSec = docTxt:find("### `%[", secEnd + 1)
        local section = docTxt:sub(secEnd, nextSec or #docTxt)
        check("nameplate section has no size param", not section:find("| `size`", 1, true))
    else
        failed = (failed or 0) + 1  -- vacuous-pass guard: no section, no pass
    end
    local pt = Schema.coerce("textbox", { w = "99999" }, {})
    check("textbox w clamped", pt.w == 4096)
end

-- [play] contract (unified audio entry)
do
    pcall(require, "kag")  -- registers the play contract
    check("play migrated", Schema.isMigrated("play"))
    local ok, err = pcall(function() Schema.coerce("play", { bus = "bogus" }, {}) end)
    check("play bogus bus throws", not ok)
    p = Schema.coerce("play", { bus = "se", file = "x.wav", volume = "9" }, {})
    check("play se + volume clamped", p.bus == "se" and p.volume == 1.5)
    p = Schema.coerce("play", { file = "y.wav" }, {})
    check("play default volume", p.volume == 1.0)
end

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

-- particles migrated
check("particles migrated", Schema.isMigrated("particles"))
p = Schema.coerce("particles", { rate = "99999" }, {})
check("particles rate clamped", p.rate == 1000)
p = Schema.coerce("particles", { sizeMax = "999" }, {})
check("particles sizeMax clamped", p.sizeMax == 512)
p = Schema.coerce("particles", { r = "300" }, {})
check("particles color clamped", p.r == 255)

-- load the command modules so their contracts register (schema.define
-- runs at module load; test_schema alone only has _test_cmd)
pcall(require, "kag.commands.video")
pcall(require, "kag.commands.save")

-- video/save/load migrated
for _, c in ipairs({ "video", "save", "load" }) do
    check(c .. " migrated", Schema.isMigrated(c))
end
p = Schema.coerce("video", { file = "x.mp4", volume = "9" }, {})
check("video volume clamped", p.volume == 1.5)
ok, err = pcall(function() Schema.coerce("video", {}, {}) end)
check("video missing file throws", not ok)
p = Schema.coerce("save", { slot = "150" }, {})
check("save slot clamped to 99", p.slot == 99)
p = Schema.coerce("save", { slot = "-5" }, {})
check("save slot clamped to 0", p.slot == 0)

-- layer commands migrated
for _, c in ipairs({ "position", "layopt", "fadeout" }) do
    check(c .. " migrated", Schema.isMigrated(c))
end
p = Schema.coerce("position", { scale = "99" }, {})
check("position scale clamped", p.scale == 16)
p = Schema.coerce("layopt", { opacity = "2" }, {})
check("layopt opacity clamped", p.opacity == 1.0)
p = Schema.coerce("fadeout", { time = "99999" }, {})
check("fadeout time clamped", p.time == 30000)

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
-- playbgm volume clamp + file/storage alias
local ok, err = pcall(function()
    Schema.coerce("playbgm", { volume = "9" }, {})
end)
check("playbgm missing file+storage throws", not ok)
p = Schema.coerce("playbgm", { storage = "x.wav", volume = "9" }, {})
check("playbgm storage alias works", p.volume == 1.5)
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
