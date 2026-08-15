-- =============================================================================
--  test_math_cmds.lua — KAG3 arithmetic commands: [add]/[sub]/[mul]/[div]/
--  [mod]/[dec] (scripts/kag/commands/math.lua).
--  Covers: per-operator arithmetic, default vs explicit operand, nil-safe
--  starts (missing variable -> 0, mirroring [inc]), all five variable scopes
--  (f./tf./sf./mp./lf.) + bare variable, value handling via tonumber, the
--  [mod]/[div]-by-zero visible "[KAG] division by zero" error + no-op, and
--  end-to-end scheduler-drive with a mocked kag table.
--  Independent runnable: external/lua/lua.exe tests/scripts/test_math_cmds.lua
-- =============================================================================
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then
        print("PASS " .. name)
    else
        print("FAIL " .. name .. " -- " .. tostring(detail))
    end
    results[#results + 1] = cond
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
local math_cmds = require("kag.commands.math")
require("kag.commands.text")  -- registers the [ch] contract (interpolation)

local function make_ctx()
    return {
        f = {}, sf = {}, tf = {}, mp = {}, lf = {},
        current_scene = "math.ks", token_index = 1,
    }
end

-- ---- each operator: add/sub/mul/div/mod --------------------------------
do
    local ctx = make_ctx()
    math_cmds.add(ctx, { name = "f.x" })  -- value nil (schema would require it) -> no-op warn
    ctx.f.x = 10
    math_cmds.add(ctx, { name = "f.x", value = 5 })
    check("add += 5", ctx.f.x == 15, tostring(ctx.f.x))
    math_cmds.sub(ctx, { name = "f.x", value = 3 })
    check("sub -= 3", ctx.f.x == 12, tostring(ctx.f.x))
    math_cmds.mul(ctx, { name = "f.x", value = 4 })
    check("mul *= 4", ctx.f.x == 48, tostring(ctx.f.x))
    math_cmds.div(ctx, { name = "f.x", value = 2 })
    check("div /= 2", ctx.f.x == 24, tostring(ctx.f.x))
    math_cmds.mod(ctx, { name = "f.x", value = 10 })
    check("mod %= 10", ctx.f.x == 4, tostring(ctx.f.x))
end

-- ---- dec: default amount == 1, explicit amount -------------------------
do
    local ctx = make_ctx()
    ctx.f.count = 10
    math_cmds.dec(ctx, { name = "f.count" })
    check("dec default amount=1", ctx.f.count == 9, tostring(ctx.f.count))
    math_cmds.dec(ctx, { name = "f.count", amount = 4 })
    check("dec explicit amount=4", ctx.f.count == 5, tostring(ctx.f.count))
end

-- ---- inc/dec twin symmetry (inc lives in system.lua) -------------------
do
    local system = require("kag.commands.system")
    local ctx = make_ctx()
    ctx.f.c = 5
    system.inc(ctx, { var = "f.c" })      -- 6
    math_cmds.dec(ctx, { name = "f.c" })  -- 5
    check("inc then dec restores", ctx.f.c == 5, tostring(ctx.f.c))
end

-- ---- nil-safe: missing variable starts at 0 -----------------------------
do
    local ctx = make_ctx()
    math_cmds.add(ctx, { name = "f.missing", value = 7 })
    check("add nil-safe start 0", ctx.f.missing == 7, tostring(ctx.f.missing))
    math_cmds.sub(ctx, { name = "f.missing", value = 2 })
    check("sub nil-safe continues", ctx.f.missing == 5, tostring(ctx.f.missing))
    math_cmds.dec(ctx, { name = "f.none" })
    check("dec nil-safe start 0 (amount 1)", ctx.f.none == -1, tostring(ctx.f.none))
end

-- ---- all five scopes + bare variable -> f -------------------------------
do
    local ctx = make_ctx()
    ctx.tf.a = 1
    ctx.sf.b = 2
    ctx.mp.c = 3
    ctx.lf.d = 4
    ctx.f.e = 5
    math_cmds.add(ctx, { name = "tf.a", value = 1 })
    math_cmds.add(ctx, { name = "sf.b", value = 1 })
    math_cmds.add(ctx, { name = "mp.c", value = 1 })
    math_cmds.add(ctx, { name = "lf.d", value = 1 })
    math_cmds.add(ctx, { name = "e", value = 1 })       -- bare -> ctx.f
    check("tf scope", ctx.tf.a == 2)
    check("sf scope", ctx.sf.b == 3)
    check("mp scope", ctx.mp.c == 4)
    check("lf scope", ctx.lf.d == 5)
    check("bare var -> f", ctx.f.e == 6)
end

-- ---- value via tonumber (string coercion, mirrors [inc]) ----------------
do
    local ctx = make_ctx()
    ctx.f.a = 10
    math_cmds.add(ctx, { name = "f.a", value = "5" })  -- string "5" -> 5
    check("value string coerced via tonumber", ctx.f.a == 15, tostring(ctx.f.a))
end

-- ---- [mod] division by zero: visible error + no-op ----------------------
do
    local ctx = make_ctx()
    ctx.f.a = 10
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    math_cmds.mod(ctx, { name = "f.a", value = 0 })
    print = realPrint
    local found = false
    for _, p in ipairs(printed) do
        if p:find("[KAG] division by zero", 1, true) and p:find("[mod ", 1, true) then
            found = true
        end
    end
    check("mod-by-zero prints [KAG] division by zero", found,
        table.concat(printed, "|"))
    check("mod-by-zero no-ops (value unchanged)", ctx.f.a == 10, tostring(ctx.f.a))
end

-- ---- [div] division by zero: same visible guard (no inf corruption) ------
do
    local ctx = make_ctx()
    ctx.f.a = 10
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    math_cmds.div(ctx, { name = "f.a", value = 0 })
    print = realPrint
    local found = false
    for _, p in ipairs(printed) do
        if p:find("[KAG] division by zero", 1, true) and p:find("[div ", 1, true) then
            found = true
        end
    end
    check("div-by-zero prints [KAG] division by zero", found,
        table.concat(printed, "|"))
    check("div-by-zero no-ops (value unchanged)", ctx.f.a == 10, tostring(ctx.f.a))
end

-- ---- unknown variable scope: warn + no-op -------------------------------
do
    local ctx = make_ctx()
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    math_cmds.add(ctx, { name = "zz.boom", value = 1 })
    print = realPrint
    local found = false
    for _, p in ipairs(printed) do
        if p:find("unknown variable scope", 1, true) then found = true end
    end
    check("unknown scope warns", found, table.concat(printed, "|"))
end

-- ---- end-to-end through the scheduler -------------------------------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    local kag = {}
    package.loaded["kag"] = kag
    kag.ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end
    kag.add = math_cmds.add
    kag.sub = math_cmds.sub
    kag.mul = math_cmds.mul
    kag.div = math_cmds.div
    kag.mod = math_cmds.mod
    kag.dec = math_cmds.dec
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "e2e.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
[add name=f.x value=10]
[add name=f.x value=5]
[sub name=f.x value=3]
[mul name=f.x value=2]
[div name=f.x value=4]
[mod name=f.x value=7]
[dec name=f.x amount=1]
[ch text="x=$f.x"]
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    -- ((0+10+5-3)*2)/4 % 7 - 1 = (24/4=6) % 7 - 1 = 6 - 1 = 5
    check("e2e arithmetic chain -> 5", ctx.f.x == 5, tostring(ctx.f.x))
    -- div yields a float (6.0), so the interpolated value may print "5.0"
    -- (Lua tostring of a whole float). Assert against either spelling.
    check("e2e interpolates result", dispatched[1]
        and (dispatched[1] == "x=5" or dispatched[1] == "x=5.0"), dispatched[1])
end

-- ---- positional (KAG3 style) [add f.x 5] --------------------------------
do
    local kag_orig = package.loaded["kag"]
    local kag = { ch = function() end }
    package.loaded["kag"] = kag
    kag.add = math_cmds.add
    kag.sub = math_cmds.sub
    kag.mul = math_cmds.mul
    kag.div = math_cmds.div
    kag.mod = math_cmds.mod
    kag.dec = math_cmds.dec
    local ctx = make_ctx()
    ctx.f.x = 1
    local tokens = tokenizer.parse("[add f.x 5]")
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    -- schema requires name (filled positionally) and value (filled positionally)
    check("positional [add f.x 5]", ctx.f.x == 6, tostring(ctx.f.x))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MATH COMMANDS TESTS DONE")
