-- test_control_flow.lua — KAG3 control-flow completeness:
-- [elsif] alias, [call *label] intra-scene calls, unknown-tag warnings.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")

-- ---- [elsif] alias normalizes in the tokenizer ----------------------------
do
    local tokens = tokenizer.parse("start\n[elsif exp=\"x\"]\nend")
    local found = false
    for _, t in ipairs(tokens) do
        if t.type == "command" and (t.cmd == "elseif" or t.cmd == "elsif") then
            found = true
            check("[elsif] normalized to elseif at parse", t.cmd == "elseif", t.cmd)
        end
    end
    check("[elsif] token present", found)
end

-- ---- [elsif] alias executes as a real branch ------------------------------
do
    local dispatched = {}
    local kag = { ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end }
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = kag
    local ctx = { f = { a = 1 }, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "cf.ks", token_index = 1,
                  stop_flag = false }
    local tokens = tokenizer.parse([[
[if exp="f.a > 10"]
[ch text="big"]
[elsif exp="f.a == 1"]
[ch text="one"]
[else]
[ch text="other"]
[endif]
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("[elsif] branch taken when condition matches",
        dispatched[1] and dispatched[1][2].text == "one",
        dispatched[1] and dispatched[1][2] and dispatched[1][2].text)
    check("[elsif] chain does not fall into else",
        #dispatched == 1, #dispatched)
end

-- ---- [call *label] intra-scene call + return ------------------------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    -- Realistic mock: only registered commands respond; unknown tags hit nil
    -- so the scheduler's unknown-tag fallback (warn + render as text) runs.
    local kag = {}
    package.loaded["kag"] = kag
    kag.ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end
    kag.eval = function(c2, p2)
        local code = p2.code or p2[1] or ""
        local var, val = code:match("^lf%.(%w+)%s*=%s*(%d+)")
        if var and val then c2.lf[var] = tonumber(val) end
    end
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = { outer = 1 },
                  current_scene = "sub.ks", token_index = 1,
                  stop_flag = false }
    local lfDuringCallee = nil
    kag.ch = function(c2, p2)
        if p2.text == "inside" then lfDuringCallee = c2.lf end
        dispatched[#dispatched + 1] = { "ch", p2 }
    end
    local callerLf = ctx.lf
    -- KAG3 convention: subroutine bodies live after the call site, so the
    -- linear fall-through after [return] skips them with a [jump].
    local tokens = tokenizer.parse([[
[ch text="before"]
[call *subroutine]
[ch text="after"]
[jump *end]
*subroutine
[ch text="inside"]
[eval exp="lf.inside = 5"]
[return]
*end
]])
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    local texts = {}
    for _, d in ipairs(dispatched) do
        if d[1] == "ch" and d[2].text then texts[#texts + 1] = d[2].text end
    end
    check("[call *label] runs before, inside, after in order",
        texts[1] == "before" and texts[2] == "inside" and texts[3] == "after",
        table.concat(texts, ","))
    check("callee lf frame isolated (inside=5, outer nil)",
        lfDuringCallee ~= nil and lfDuringCallee ~= callerLf
            and lfDuringCallee.inside == 5 and lfDuringCallee.outer == nil,
        "inside=" .. tostring(lfDuringCallee and lfDuringCallee.inside)
            .. " outer=" .. tostring(lfDuringCallee and lfDuringCallee.outer))
    check("caller lf restored after return",
        ctx.lf == callerLf and ctx.lf.inside == nil, "outer=" .. tostring(ctx.lf.outer))
    check("no re-run of the callee body after return",
        texts[4] == nil, texts[4])

    -- nested [call *label]: call a second label from inside the first
    local d2 = {}
    local kag2 = { ch = function(c2, p2) d2[#d2 + 1] = { "ch", p2 } end }
    package.loaded["kag"] = kag2
    local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                   current_scene = "n.ks", token_index = 1, stop_flag = false }
    local tokens2 = tokenizer.parse([[
[call *a]
[jump *end]
*a
[call *b]
[ch text="a-body"]
[return]
*b
[ch text="b-body"]
[return]
*end
]])
    local co2 = coroutine.create(function()
        scheduler.run(ctx2, tokens2, 1)
    end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    package.loaded["kag"] = kag_orig
    local t2 = {}
    for _, d in ipairs(d2) do
        if d[1] == "ch" and d[2].text then t2[#t2 + 1] = d[2].text end
    end
    check("nested [call *label] order b-body, a-body (no re-run)",
        t2[1] == "b-body" and t2[2] == "a-body" and t2[3] == nil,
        table.concat(t2, ","))
end

-- ---- unknown tag: warn + render as text -----------------------------------
do
    local dispatched = {}
    local printed = {}
    local kag_orig = package.loaded["kag"]
    local realPrint = print
    local kag = {}
    package.loaded["kag"] = kag
    kag.ch = function(c2, p2) dispatched[#dispatched + 1] = { "ch", p2 } end
    print = function(...) printed[#printed + 1] = table.concat({...}, " ") end
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "warn.ks", token_index = 4, stop_flag = false }
    local tokens = tokenizer.parse("[totally_unknown_tag x=1]")
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    print = realPrint
    package.loaded["kag"] = kag_orig
    check("unknown tag rendered as text",
        dispatched[1] and dispatched[1][1] == "ch"
            and dispatched[1][2].text:find("totally_unknown_tag", 1, true) ~= nil,
        dispatched[1] and dispatched[1][2] and dispatched[1][2].text)
    local warned = false
    for _, p in ipairs(printed) do
        if p:find("[WARN] unknown KAG command", 1, true)
            and p:find("warn.ks:4", 1, true) then warned = true end
    end
    check("unknown tag warning printed with location", warned,
        table.concat(printed, " | "))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("CONTROL FLOW TESTS DONE")
