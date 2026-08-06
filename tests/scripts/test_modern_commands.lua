-- test_modern_commands.lua — Neo-Genesis utility commands:
-- [set] [inc] [random] [assert] + typed variable semantics.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
local system = require("kag.commands.system")
require("kag.commands.text")  -- registers the [ch] contract (interpolation)

-- ---- [set] typed assignment ----------------------------------------------
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "m.ks", token_index = 1 }
    system.set(ctx, { var = "f.hp", value = "30" })
    check("[set] integer", ctx.f.hp == 30, tostring(ctx.f.hp))
    system.set(ctx, { var = "f.name", value = "Aoi" })
    check("[set] string", ctx.f.name == "Aoi")
    system.set(ctx, { var = "tf.flag", value = "true" })
    check("[set] boolean", ctx.tf.flag == true)
    system.set(ctx, { var = "sf.ratio", value = "0.5" })
    check("[set] decimal", ctx.sf.ratio == 0.5)
    system.set(ctx, { var = "bare", value = "7" })
    check("[set] bare var goes to f", ctx.f.bare == 7)
    system.set(ctx, { var = "lf.local", value = "x" })
    check("[set] lf scope", ctx.lf["local"] == "x")
end

-- ---- [inc] increment / by / nil-safe -------------------------------------
do
    local ctx = { f = { count = 5 }, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "m.ks", token_index = 2 }
    system.inc(ctx, { var = "f.count" })
    check("[inc] +1", ctx.f.count == 6)
    system.inc(ctx, { var = "f.count", by = 3 })
    check("[inc] +by", ctx.f.count == 9)
    system.inc(ctx, { var = "f.missing" })
    check("[inc] nil-safe start at 0", ctx.f.missing == 1)
    system.inc(ctx, { var = "f.count", by = -4 })
    check("[inc] negative by (decrement)", ctx.f.count == 5)
end

-- ---- [random] range + writes ---------------------------------------------
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "m.ks", token_index = 3 }
    local okRange = true
    for _ = 1, 200 do
        system.random(ctx, { var = "f.dice", min = 1, max = 6 })
        if ctx.f.dice < 1 or ctx.f.dice > 6 or ctx.f.dice % 1 ~= 0 then
            okRange = false
        end
    end
    check("[random] 200 rolls within [1,6]", okRange, tostring(ctx.f.dice))
    system.random(ctx, { var = "f.swap", min = 10, max = 1 })
    check("[random] min>max swapped", ctx.f.swap >= 1 and ctx.f.swap <= 10)
end

-- ---- [assert] pass / fail ------------------------------------------------
do
    local ctx = { f = { hp = 10 }, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "m.ks", token_index = 4 }
    -- passing assert: no error, no output
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    system.assert(ctx, { exp = "f.hp > 0", msg = "ok" })
    print = realPrint
    check("[assert] passes silently", #printed == 0, table.concat(printed, "|"))

    -- failing assert: prints diagnostic and raises
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    local ok, err = pcall(system.assert, ctx, { exp = "f.hp < 0", msg = "hp bad" })
    print = realPrint
    check("[assert] fails and raises", ok == false and err:find("hp bad", 1, true) ~= nil,
        tostring(err))
    local diag = false
    for _, p in ipairs(printed) do
        if p:find("%[KAG%] %[assert%]", 1) and p:find("m.ks:4", 1, true) then diag = true end
    end
    check("[assert] diagnostic has location", diag, table.concat(printed, "|"))

    -- TJS operator inside assert expression (expr.lua integration)
    local ok2 = pcall(system.assert, ctx, { exp = "f.hp > 0 && f.hp < 100" })
    check("[assert] TJS && expression works", ok2)
end

-- ---- end-to-end through the scheduler ------------------------------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    local kag = {}
    package.loaded["kag"] = kag
    kag.ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end
    kag.set = system.set
    kag.inc = system.inc
    kag.assert = system.assert
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "e2e.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
[set var="f.hp" value="30"]
[inc var="f.hp"]
[assert exp="f.hp == 31"]
[ch text="hp is $f.hp"]
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("e2e: set+inc+assert then ch interpolates",
        dispatched[1] and dispatched[1][2].text == "hp is 31",
        dispatched[1] and dispatched[1][2] and dispatched[1][2].text)
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MODERN COMMANDS TESTS DONE")
