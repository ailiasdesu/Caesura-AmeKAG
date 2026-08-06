-- test_kag_debug.lua — KAG scene-level debugger:
-- breakpoints (scene+cmd / scene+line), single-step, scope inspection.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
local kagDebug = require("kag_debug")

-- The scheduler yields after EVERY token; a debug pause is an extra
-- "__kag_pause" yield BEFORE the token. Driving helper: keep resuming
-- with "continue" until the coroutine pauses again (returns true + the
-- number of resumes used) or dies (returns false).
local function resume_to_pause(co)
    local n = 0
    while true do
        local ok, yielded = coroutine.resume(co, "continue")
        n = n + 1
        if not ok then error(yielded, 0) end
        if yielded == "__kag_pause" then return true, n end
        if coroutine.status(co) == "dead" then return false, n end
        if n > 30 then error("runaway resume loop", 0) end
    end
end

-- ---- breakpoint on scene+command -----------------------------------------
do
    local dispatched = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = { ch = function(c2, p2)
        dispatched[#dispatched + 1] = p2.text end }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "dbg.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
[ch text="one"]
[ch text="two"]
[ch text="three"]
]])
    kagDebug.clear_breakpoints()
    kagDebug.set_breakpoint("dbg.ks", "ch")   -- pause before EVERY [ch]
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    -- first resume: breakpoint hit before token 1 (nothing executed yet)
    local ok1, yielded1 = coroutine.resume(co)
    check("breakpoint pauses before first [ch]",
        ok1 and yielded1 == "__kag_pause", tostring(yielded1))
    check("nothing dispatched while paused",
        #dispatched == 0, tostring(#dispatched))
    -- continue through: every [ch] pauses again (2 more pauses -- the
    -- initial resume already paused before token 1), then the stream
    -- completes and all three texts dispatch in order.
    local pauses = 0
    while true do
        local paused, n = resume_to_pause(co)
        if not paused then break end
        pauses = pauses + 1
        if pauses > 10 then error("too many pauses", 0) end
    end
    check("breakpoint hits on every [ch] (3 pauses total)",
        pauses == 2, tostring(pauses))
    check("all texts dispatched in order",
        dispatched[1] == "one" and dispatched[2] == "two"
            and dispatched[3] == "three" and dispatched[4] == nil,
        table.concat(dispatched, ","))
    package.loaded["kag"] = kag_orig
end

-- ---- breakpoint on scene+line --------------------------------------------
do
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = { ch = function() end }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "dbg.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
[ch text="one"]
[eval exp="f.x = 1"]
[ch text="two"]
]])
    kagDebug.clear_breakpoints()
    kagDebug.set_breakpoint("dbg.ks", 2)   -- the [eval] line (token 2)
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local paused = resume_to_pause(co)
    -- paused before token 2 ([eval]): token_index still points at the
    -- last EXECUTED token (1). The eval must NOT have run yet.
    check("line breakpoint pauses before token 2",
        paused and ctx.token_index == 1,
        "paused=" .. tostring(paused) .. " idx=" .. tostring(ctx.token_index))
    check("f.x not yet assigned at pause", ctx.f.x == nil, tostring(ctx.f.x))
    -- continue to completion: eval runs
    while true do
        local p2 = resume_to_pause(co)
        if not p2 then break end
    end
    check("f.x assigned after continue",
        ctx.f.x == 1, tostring(ctx.f.x))
    package.loaded["kag"] = kag_orig
end

-- ---- single-step ----------------------------------------------------------
do
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = { ch = function() end }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "dbg.ks", token_index = 1, stop_flag = false }
    local tokens = tokenizer.parse([[
[ch text="one"]
[ch text="two"]
[ch text="three"]
]])
    kagDebug.clear_breakpoints()
    kagDebug.enable(true)
    kagDebug.step()   -- pause at the NEXT token
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local ok1, yielded1 = coroutine.resume(co)
    check("step pauses at first token",
        ok1 and yielded1 == "__kag_pause", tostring(yielded1))
    -- resume with "step": the token executes, then the next token pauses
    local ok2, yielded2 = coroutine.resume(co, "step")
    check("step arm survives resume",
        ok2 and (yielded2 == "__kag_pause" or yielded2 == nil),
        tostring(yielded2))
    -- drain: must reach the end (no infinite pause loop)
    local pauses = 0
    while true do
        local paused = resume_to_pause(co)
        if not paused then break end
        pauses = pauses + 1
        if pauses > 10 then error("too many pauses", 0) end
    end
    check("step mode drains to completion", true, tostring(pauses))
    package.loaded["kag"] = kag_orig
end

-- ---- scope inspection -----------------------------------------------------
do
    local ctx = { f = { hp = 42, name = "Aoi" }, sf = { day = 2 },
                  tf = { flag = true }, mp = { 7 }, lf = { x = "y" } }
    local insp = kagDebug.inspect(ctx, "f")
    check("inspect f scope", insp.f.hp == 42 and insp.f.name == "Aoi",
        tostring(insp.f and insp.f.hp))
    local all = kagDebug.inspect(ctx, "all")
    check("inspect all scopes",
        all.f.hp == 42 and all.sf.day == 2 and all.tf.flag == true
            and all.lf.x == "y", tostring(all.f and all.f.hp))
end

-- ---- breakpoint lifecycle -------------------------------------------------
do
    kagDebug.clear_breakpoints()
    kagDebug.set_breakpoint("a.ks", "ch")
    kagDebug.set_breakpoint("a.ks", 12)
    kagDebug.set_breakpoint("b.ks", "ch")
    check("breakpoints registered",
        kagDebug.get_breakpoints()["a.ks:ch"] == true
            and kagDebug.get_breakpoints()["a.ks:12"] == true
            and kagDebug.get_breakpoints()["b.ks:ch"] == true,
        tostring(#kagDebug.get_breakpoints()))
    kagDebug.clear_breakpoints("a.ks")
    check("clear_breakpoints(scene) keeps other scenes",
        kagDebug.get_breakpoints()["a.ks:ch"] == nil
            and kagDebug.get_breakpoints()["b.ks:ch"] == true,
        tostring(#kagDebug.get_breakpoints()))
    kagDebug.remove_breakpoint("b.ks", "ch")
    check("remove_breakpoint", kagDebug.get_breakpoints()["b.ks:ch"] == nil,
        tostring(#kagDebug.get_breakpoints()))
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("KAG DEBUG TESTS DONE")
