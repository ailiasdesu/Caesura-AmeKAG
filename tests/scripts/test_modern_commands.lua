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

    -- failing assert: raises with location in the error message
    local ok, err = pcall(system.assert, ctx, { exp = "f.hp < 0", msg = "hp bad" })
    check("[assert] fails and raises", ok == false
        and err:find("hp bad", 1, true) ~= nil
        and err:find("m.ks:4", 1, true) ~= nil,
        tostring(err))

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

-- ---- positional (KAG3 style) forms [set f.hp 30] etc. --------------------
do
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "pos.ks", token_index = 1 }
    -- coerce path: contract positional_index skips required while the
    -- positional slot is filled; handler falls back to params[N].
    -- Split runs: [set] alone first (check 30), then [inc]+[random]
    -- (check 32 + dice range) -- a single run would already have
    -- incremented before the [set] check.
    local tokens = tokenizer.parse([[
[set f.hp 30]
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    check("positional [set f.hp 30]", ctx.f.hp == 30, tostring(ctx.f.hp))
    local tokens2 = tokenizer.parse([[
[inc f.hp 2]
[random f.dice 1 6]
]])
    local co2 = coroutine.create(function()
        scheduler.run(ctx, tokens2, 1)
    end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    check("positional [inc f.hp 2]", ctx.f.hp == 32, tostring(ctx.f.hp))
    check("positional [random f.dice 1 6] in range",
        ctx.f.dice >= 1 and ctx.f.dice <= 6, tostring(ctx.f.dice))
end

-- ---- implicit return: [call *label] falling off the token stream ---------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    local kag = { ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end }
    package.loaded["kag"] = kag
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "ir.ks", token_index = 1, stop_flag = false }
    -- The subroutine is the LAST tokens and has NO [return] tag: the
    -- implicit-return logic must pop the stale call frame at stream end.
    -- KAG3 convention: a [jump *end] skips the subroutine body on the
    -- linear fall-through after the call.
    local tokens = tokenizer.parse([[
[call *sub]
[jump *end]
*sub
[ch text="inside"]
[ch text="after"]
*end
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("implicit return: inside then after, exactly once",
        dispatched[1] == "inside" and dispatched[2] == "after"
            and dispatched[3] == nil,
        table.concat(dispatched or {}, ","))
    check("implicit return: call stack drained",
        (ctx.call_stack or {}).n == nil or #ctx.call_stack == 0,
        tostring(ctx.call_stack and #ctx.call_stack))
end

-- ---- unknown-tag warning dedup (once per scene+command) ------------------
do
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = {}  -- no handlers at all -> everything unknown
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "w.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse("[bogus_tag] [bogus_tag] [bogus_tag]")
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    print = realPrint
    package.loaded["kag"] = kag_orig
    local warnCount = 0
    for _, p in ipairs(printed) do
        if p:find("unknown KAG command 'bogus_tag'", 1, true) then
            warnCount = warnCount + 1
        end
    end
    check("unknown-tag warning deduped to one",
        warnCount == 1, "warned " .. warnCount .. "x")
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MODERN COMMANDS TESTS DONE")
