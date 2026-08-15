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

-- ---- [notify] routes to a mocked toast module with correct args --------
do
    local calls = {}
    local toast_orig = package.loaded["toast"]
    package.loaded["toast"] = {
        show = function(msg, ttl) calls[#calls + 1] = { msg, ttl } end,
    }
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "notify.ks", token_index = 1 }

    -- explicit time in ms -> toast ttl in SECONDS (100ms -> 0.1s)
    local rc = system.notify(ctx, { msg = "存档完成", time = 100 })
    check("[notify] routes to toast with ms->s ttl", rc == true
        and calls[1] and calls[1][1] == "存档完成" and calls[1][2] == 0.1,
        table.concat(({ tostring(calls[1] and calls[1][1]), tostring(calls[1] and calls[1][2]) }), ","))

    -- omitted time -> schema default 2500ms -> 2.5s
    system.notify(ctx, { msg = "no time" })
    check("[notify] default time 2500ms -> 2.5s", calls[2]
        and calls[2][2] == 2.5, tostring(calls[2] and calls[2][2]))

    -- tonal kind is accepted (reserved metadata; toast has no kind support)
    system.notify(ctx, { msg = "warn look", time = 500, kind = "warn" })
    check("[notify] kind accepted without effect", rc == true
        and calls[3] and calls[3][2] == 0.5, tostring(calls[3] and calls[3][2]))

    -- clamping: out-of-range time is clamped, not past to toast
    system.notify(ctx, { msg = "big", time = 999999 })
    check("[notify] time clamped to 30000ms -> 30s", calls[4]
        and calls[4][2] == 30.0, tostring(calls[4] and calls[4][2]))

    package.loaded["toast"] = toast_orig
end

-- ---- [notify] degrades gracefully when toast is unavailable --------------
do
    local orig = package.loaded["toast"]
    package.loaded["toast"] = {}  -- stub without `show`: models headless/no-UI (require() would reload real toast.lua if cleared to nil)
    local printed = {}
    local realPrint = print
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "notify.ks", token_index = 2 }
    local ok, rc = pcall(system.notify, ctx, { msg = "saved" })
    -- must NOT raise into the caller and must not block
    check("[notify] headless toast-unavailable degrades (returns true)",
        ok and rc == true, tostring(rc))
    local sawDegrade = false
    for _, p in ipairs(printed) do
        if p:find("toast unavailable", 1, true) then sawDegrade = true end
    end
    check("[notify] degradation notice printed", sawDegrade, table.concat(printed, "|"))
    print = realPrint
    package.loaded["toast"] = orig
end

-- ---- [notify] degrades when toast.show itself raises ----------------------
do
    local toast_orig = package.loaded["toast"]
    package.loaded["toast"] = {
        show = function() error("GPU down") end,
    }
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "notify.ks", token_index = 3 }
    local ok, rc = pcall(system.notify, ctx, { msg = "boom", time = 100 })
    check("[notify] toast.show raise is swallowed (returns true)",
        ok and rc == true, tostring(rc))
    package.loaded["toast"] = toast_orig
end

-- ---- [notify] contract: missing msg raises at coerce; defaults apply ------
do
    local schema = require("kag.schema")
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "notify.ks", token_index = 4 }
    -- missing required msg -> coerce raises
    local ok, err = pcall(schema.coerce, "notify", { time = 100 }, ctx)
    check("[notify] coerce rejects missing msg", ok == false
        and err:find("msg", 1, true) ~= nil, tostring(err))
    -- present msg -> defaults injected (time 2500, kind absent)
    local out = schema.coerce("notify", { msg = "hi" }, ctx)
    check("[notify] coerce applies time default 2500", out.msg == "hi"
        and out.time == 2500, tostring(out.time))
end

-- ---- [notify] reachable end-to-end through the scheduler ------------------
do
    local calls = {}
    local toast_orig = package.loaded["toast"]
    package.loaded["toast"] = {
        show = function(msg, ttl) calls[#calls + 1] = { msg, ttl } end,
    }
    local ctx = { f = {}, sf = {}, tf = {}, mp = {}, lf = {},
                  current_scene = "n.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse("[notify msg=\"saved via scheduler\" time=100]")
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["toast"] = toast_orig
    check("[notify] scheduler dispatches to handler + toast",
        calls[1] and calls[1][1] == "saved via scheduler" and calls[1][2] == 0.1,
        table.concat(({ tostring(calls[1] and calls[1][1]), tostring(calls[1] and calls[1][2]) }), ","))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MODERN COMMANDS TESTS DONE")
