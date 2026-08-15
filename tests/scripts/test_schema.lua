-- test_schema.lua — Neo-Genesis command contract tests
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
    check("bare text coerced as ch", sched:find('schemaModule.coerce("ch", params, ctx)', 1, true) ~= nil)
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

-- ${expr} balanced-brace scanning (round 54: old [^{}]+ pattern
-- truncated table-constructor expressions and leaked the raw span)
do
    local ctx = { f = { hp = 42, name = "Ame" } }
    local p2 = Schema.coerce("_interp_test",
        { text = "v ${ {a=1,b=2}.b } ok ${ {1,2,3}[2] }" }, ctx)
    check("nested table constructor interpolates", p2.text == "v 2 ok 2")
    local p3 = Schema.coerce("_interp_test",
        { text = "q ${ \"}\" .. f.name }" }, ctx)
    check("quoted brace does not close span", p3.text == "q }Ame")
    local p4 = Schema.coerce("_interp_test",
        { text = "unterm ${oops stays" }, ctx)
    check("unterminated span stays verbatim", p4.text == "unterm ${oops stays")
    local p5 = Schema.coerce("_interp_test",
        { text = "nested ${ {a=1}.a + f.hp }" }, ctx)
    check("constructor + var arithmetic", p5.text == "nested 43")
    local p6 = Schema.coerce("_interp_test",
        { text = "empty ${} ok" }, ctx)
    check("empty span stays verbatim", p6.text == "empty ${} ok")
end

-- ${expr} Lua long brackets (round 62): braces inside [[...]] /
-- [=[...]=] must not close the span early
do
    local ctx = { f = { name = "Ame" } }
    local p7 = Schema.coerce("_interp_test",
        { text = "l ${ [[}}]] .. f.name }" }, ctx)
    check("long-bracket braces do not close span", p7.text == "l }}Ame", p7.text)
    local p8 = Schema.coerce("_interp_test",
        { text = "l ${ [=[}]=] .. f.name }" }, ctx)
    check("level-1 long bracket braces skipped", p8.text == "l }Ame", p8.text)
    local p9 = Schema.coerce("_interp_test",
        { text = "plain ${ f.name }" }, ctx)
    check("regression: plain expr unaffected", p9.text == "plain Ame")
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

-- positional set*volume clamps (the default-removal regression class)
do
    local src = io.open("scripts/kag/commands/audio.lua", "r")
    local a = src and src:read("*a") or ""
    if src then src:close() end
    check("set*volume clamp helper present", a:find("clampVolume", 1, true) ~= nil)
    check("clamp bound 1.5", a:find("math.min(1.5", 1, true) ~= nil)
    check("clamp bound 0", a:find("math.max(0", 1, true) ~= nil)
end

-- sprite performance commands (fade/move)
do
    pcall(require, "kag.commands.text")
    check("sprite_fade migrated", Schema.isMigrated("sprite_fade"))
    check("sprite_move migrated", Schema.isMigrated("sprite_move"))
    local ok, err = pcall(function() Schema.coerce("sprite_fade", {}, {}) end)
    check("sprite_fade speaker required", not ok)
    local ps = Schema.coerce("sprite_fade", { speaker = "A", to = "999", time = "99999" }, {})
    check("sprite_fade to/time clamped", ps.to == 255 and ps.time == 30000)
    ps = Schema.coerce("sprite_move", { speaker = "A", x = "0", y = "0", time = "-5" }, {})
    check("sprite_move time min clamped", ps.time == 0)
    local src = io.open("scripts/kag/commands/text.lua", "r")
    local t = src and src:read("*a") or ""
    if src then src:close() end
    check("sprite_fade no-layer guard", t:find("no sprite layer", 1, true) ~= nil)
end

-- [ch] sprite= param consumed by the handler (was dead — review)
do
    local src = io.open("scripts/kag/commands/text.lua", "r")
    local t = src and src:read("*a") or ""
    if src then src:close() end
    check("ch handler reads params.sprite", t:find('params.sprite ~= "" and params.sprite', 1, true) ~= nil)
    check("ch guard admits sprite-only", t:find("params.sprite and params.sprite ~=", 1, true) ~= nil)
end

-- set_screen_offset reachable under strict sandbox (review regression)
do
    local src = io.open("scripts/sandbox.lua", "r")
    local snd = src and src:read("*a") or ""
    if src then src:close() end
    check("RENDER_WHITELIST has set_screen_offset",
          snd:find("RENDER_WHITELIST", 1, true) ~= nil
          and snd:find("set_screen_offset%s*=%s*true", 1) ~= nil)
    check("_G_whitelist lacks it (moved)",
          not snd:match("_G_whitelist.-}") or not snd:sub(1, snd:find("RENDER_WHITELIST") or 0):find("set_screen_offset", 1, true))
    local fac = io.open("scripts/backend_factory.lua", "r")
    local f2 = fac and fac:read("*a") or ""
    if fac then fac:close() end
    check("factory dispatch case present", f2:find('cmd == "set_screen_offset"', 1, true) ~= nil)
end

-- set_screen_offset runtime path (fractional + negative args)
do
    local src = io.open("src/script/bindings/RenderBinding.cpp", "r")
    local rb = src and src:read("*a") or ""
    if src then src:close() end
    check("binding uses checknumber", rb:find("luaL_checknumber", 1, true) ~= nil)
    check("binding rounds", rb:find("llround", 1, true) ~= nil)
    local core = io.open("src/render/BgfxDeviceCore.cpp", "r")
    local cc = core and core:read("*a") or ""
    if core then core:close() end
    check("core clamps negatives", cc:find("max<int32_t>(0", 1, true) ~= nil)
    check("core uint16 safe", cc:find("static_cast<uint16_t>(vx)", 1, true) ~= nil)
    local cam = io.open("scripts/kag/commands/transition.lua", "r")
    local tc = cam and cam:read("*a") or ""
    if cam then cam:close() end
    check("camera min 0", tc:find('min = 0, max = 2000', 1, true) ~= nil)
end

-- [sprite_scale] contract (zoom — bundled with the swap commit)
do
    check("sprite_scale migrated", Schema.isMigrated("sprite_scale"))
    local ps = Schema.coerce("sprite_scale", { speaker = "A", scale = "99" }, {})
    check("sprite_scale scale clamped", ps.scale == 4.0)
    ps = Schema.coerce("sprite_scale", { speaker = "A", scale = "-1" }, {})
    check("sprite_scale min clamped", ps.scale == 0.1)
end

-- [sprite_swap] contract (re-dress)
do
    pcall(require, "kag.commands.text")
    check("sprite_swap migrated", Schema.isMigrated("sprite_swap"))
    local ok, err = pcall(function() Schema.coerce("sprite_swap", { speaker = "A" }, {}) end)
    check("sprite_swap sprite required", not ok)
    ok, err = pcall(function() Schema.coerce("sprite_swap", { sprite = "x.png" }, {}) end)
    check("sprite_swap speaker required", not ok)
    local src = io.open("scripts/kag/commands/text.lua", "r")
    local t = src and src:read("*a") or ""
    if src then src:close() end
    check("sprite_swap load-guard", t:find("failed to load", 1, true) ~= nil)
end

-- [voice_wait] contract + click-detection fix (review regression)
do
    pcall(require, "kag")  -- voice_wait registers here (standalone-safe)
    check("voice_wait migrated", Schema.isMigrated("voice_wait"))
    local pv = Schema.coerce("voice_wait", { timeout = "1" }, {})
    -- Unknown params pass through by design; the doc row is gone instead.
    local doc = io.open("docs/api/command-contracts.md", "r")
    local dt = doc and doc:read("*a") or ""
    if doc then doc:close() end
    local vs = dt:match("### `%[voice_wait%]`(.-)### `%[") or ""
    check("voice_wait docs have no timeout row", not vs:find("timeout", 1, true))
    -- (timeout was dropped: unreachable in this runner -- see review)
    local src = io.open("scripts/kag/commands/audio.lua", "r")
    local a = src and src:read("*a") or ""
    if src then src:close() end
    check("voice_wait uses waiting_input", a:find("ctx.waiting_input", 1, true) ~= nil)
    check("voice_wait no dead deadline", not a:find("os.time() > deadline", 1, true))
    check("voice_wait blocks while waiting", a:find("ctx.waiting_input = true", 1, true) ~= nil)
    check("voice_wait skips on clear", a:find("not ctx.waiting_input", 1, true) ~= nil)
    check("voice_wait no KAG_onClick trap", not a:find("_G._KAG_onClick or", 1, true))
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
    check("play volume no default (positional)", p.volume == nil)
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
-- min is -2 (system slots -1/-2 flow through; deeper negatives clamp)
check("save slot min -2", p.slot == -2)

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
