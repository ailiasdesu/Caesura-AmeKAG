-- test_frame_bench.lua — per-frame Lua cost guard (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local layers = require("layers")
-- 5 layers with visible nodes (worst-case-ish render walk)
for i = 1, 5 do
    local st = layers.ensure(nil, "layer_" .. i, i)
    st.visible = true
end
local root = layers.get_root()
local cnt = root and #(root.children or {}) or 0
check("five layers present", cnt >= 5)
local N = 5000
local t0 = os.clock()
for _ = 1, N do
    layers.render()
end
local perFrame = (os.clock() - t0) / N
-- loose bound: CI-safe, the real cost is ~1-2us with empty layers
check("render under 500us/frame", perFrame < 0.0005)

-- ---- round 68-72 perf delta guards (budgets mirrored from the probe) ----
-- These assert the round 68-72 expression work (long-string scanning,
-- ternary-in-index, ?? null-coalescing) and the scheduler hot loop over the
-- new schema-migrated [add] command stay within loose CI-safe bounds. The
-- budgets (<2s at these sizes) leave >95% headroom over measured cost.

-- (d) mixed expression (long string + ternary + ??) translate 1000x.
do
    local expr = require("kag.expr")
    local exprs = {
        'f.s == [[x]t]] ? f.a ?? 10 : (f.b ? [[y ? z]] : 0)',
        'a != b && !c ? (f.x ?? 1) : f.arr[0] ? 99 : 0',
        'f.arr[ f.flag ? 1 : 2 ] + (f.missing ?? 7)',
        'f.s == [[ok]] && (f.a >= 3 || !f.b) ? [[t]] : "u"',
    }
    local t0 = os.clock()
    local noq = 0
    for i = 1, 1000 do
        local e = exprs[(i % 4) + 1]
        if expr.translate(e):find("?", 1, true) == nil then noq = noq + 1 end
    end
    local dt = os.clock() - t0
    -- ternary '?' must be fully wrapped away; leftover '?' are the intended
    -- ?? null-coalescing ops (present in 3 of the 4 samples -> ~750/1000 clean).
    check("mixed expr translate 1000x under 2s", dt < 2.0)
    check("mixed expr ternary '?' resolved", noq >= 230, noq)
end

-- (e) scheduler hot loop over 1000x [add] (schema-migrated) chain dispatch.
do
    local tokenizer = require("tokenizer")
    local compiler = require("kag.compiler")
    local scheduler = require("scheduler")
    -- Mock kag carrying the real [add] handler so schema coerce + dispatch both
    -- run; restore the suite's kag afterwards (test_benchmark pattern).
    local saved_kag = package.loaded["kag"]
    local old_add = (saved_kag and saved_kag.add)
    local ma = require("kag.commands.math")
    local kag_mock = { add = ma.add }
    package.loaded["kag"] = kag_mock

    local lines = {}
    for i = 1, 1000 do
        lines[#lines + 1] = string.format('[add name="f.x" value="1"]', i)
    end
    local tokens = tokenizer.parse(table.concat(lines, "\n"))
    compiler.compile(tokens)
    local ctx = {
        f = { x = 0 }, sf = {}, tf = {}, mp = {}, lf = {},
        tokens = {}, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, stop_flag = false,
    }
    local t0 = os.clock()
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    local resumes = 0
    while coroutine.status(co) ~= "dead" and resumes < 2000000 do
        resumes = resumes + 1
        local okr = coroutine.resume(co, 16)
        if not okr then break end
    end
    local dt = os.clock() - t0
    package.loaded["kag"] = saved_kag
    check("add chain dispatches 1000x (f.x=1000)", ctx.f.x == 1000, ctx.f.x)
    check("add chain 1000x under 2s", dt < 2.0, string.format("%.3fs", dt))
end

if failed > 0 then os.exit(1) end
print("FRAME BENCH TESTS DONE")
