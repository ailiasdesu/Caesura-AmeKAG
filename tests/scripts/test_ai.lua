-- test_ai.lua — AI dialogue degraded path + [ai_dialog] command:
-- without a running LLM service the binding reports unavailable and the
-- command falls back to fallback= text (never a script error).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

-- ---- backend wrappers degrade without the C++ AI binding -----------------
do
    local backend = require("backend")
    check("ai_available false without binding",
        backend.ai_available() == false, tostring(backend.ai_available()))
    local t, e = backend.ai_query("hi")
    check("ai_query degrades to nil+err",
        t == nil and e == "no-binding", tostring(t) .. "/" .. tostring(e))
    check("ai_query_async false without binding",
        backend.ai_query_async("hi", {}, function() end) == false)
    check("ai_cancel false without binding",
        backend.ai_cancel() == false)
end

-- ---- [ai_dialog] with fallback text --------------------------------------
do
    local tokenizer = require("tokenizer")
    local scheduler = require("scheduler")
    local sys = require("kag.commands.system")
    require("kag.commands.text")  -- ch contract
    local kag_orig = package.loaded["kag"]
    local dispatched = {}
    package.loaded["kag"] = {
        ai_dialog = function(c2, p2) return sys.ai_dialog(c2, p2) end,
        ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end,
    }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {}, characters = {},
                  current_scene = "ai.ks", token_index = 1, stop_flag = false }
    local co = coroutine.create(function()
        scheduler.run(ctx, tokenizer.parse([[
[ai_dialog name="Aoi" prompt="你好" fallback="（AI 服务未连接）"]
]]), 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    check("[ai_dialog] fallback text shown",
        dispatched[1] == "（AI 服务未连接）",
        tostring(dispatched[1]))
    package.loaded["kag"] = kag_orig
end

-- ---- [ai_dialog] without fallback: visible placeholder --------------------
do
    local tokenizer = require("tokenizer")
    local scheduler = require("scheduler")
    local sys = require("kag.commands.system")
    local kag_orig = package.loaded["kag"]
    local dispatched = {}
    package.loaded["kag"] = {
        ai_dialog = function(c2, p2) return sys.ai_dialog(c2, p2) end,
        ch = function(c2, p2) dispatched[#dispatched + 1] = p2.text end,
    }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {}, characters = {},
                  current_scene = "ai.ks", token_index = 1, stop_flag = false }
    local co = coroutine.create(function()
        scheduler.run(ctx, tokenizer.parse([[
[ai_dialog name="Aoi" prompt="你好"]
]]), 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    check("[ai_dialog] placeholder when no fallback",
        dispatched[1] ~= nil and #dispatched[1] > 0,
        tostring(dispatched[1]))
    package.loaded["kag"] = kag_orig
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("AI TESTS DONE")
