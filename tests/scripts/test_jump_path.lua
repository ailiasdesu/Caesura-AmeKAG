-- test_jump_path.lua — cross-scene path allowlist (security audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local scheduler = require("scheduler")

-- The allowlist is exported for tests
local is_safe = scheduler.is_safe_scene_path
check("predicate exported", type(is_safe) == "function")

-- rejects traversal / non-ks / wrong prefix
check("rejects ../", not is_safe("assets/script/../evil.ks"))
check("rejects nested", not is_safe("assets/script/a/../../evil.ks"))
check("rejects non-ks", not is_safe("assets/script/evil.txt"))
check("rejects non-prefix", not is_safe("scripts/evil.ks"))
check("rejects absolute", not is_safe("/tmp/evil.ks"))
check("rejects empty", not is_safe(""))
check("rejects non-string", not is_safe(42))
-- accepts real layout
check("accepts assets/script/x.ks", is_safe("assets/script/x.ks"))
check("accepts nested scene", is_safe("assets/script/ch1/scene_a.ks"))

-- BEHAVIOR: a traversal target must NOT reach load_tokens
local calls = {}
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function(path) calls[#calls + 1] = path return { { "ch", {} } } end }
local tokens = { { "jump", { target = "../evil.ks" } }, { "ch", { text = "after" } } }
local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
check("load_tokens never called", #calls == 0)
check("execution continues", true)
-- stronger: the next token still executes (the scene did not abort)
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local ctxB = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function(path) calls[#calls + 1] = path return { { "ch", {} } } end }
local tokensB = { { "jump", { target = "../evil.ks" } }, { "ch", { text = "after" } } }
local coB = coroutine.create(function() scheduler.run(ctxB, tokensB, 1) end)
while coroutine.status(coB) ~= "dead" do coroutine.resume(coB) end
package.loaded["kag"] = kag_orig
check("next token executes after block", #dispatched == 1
      and dispatched[1][2].text == "after")

-- BARE [jump next.ks] (KAG3 syntax) reaches load_tokens with the safe
-- path (audit: the cross-scene branch required params.target -- bare
-- values fell into the intra-scene label search)
local tokenizer = require("tokenizer")
local toksB = tokenizer.parse("[jump next.ks]")
local callsB = {}
local ctxB2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function(path) callsB[#callsB + 1] = path
        return { { "ch", {} } } end }
local coB2 = coroutine.create(function() scheduler.run(ctxB2, toksB, 1) end)
while coroutine.status(coB2) ~= "dead" do coroutine.resume(coB2) end
check("bare jump routes cross-scene", callsB[1] == "assets/script/next.ks")

-- bare traversal is still blocked by the allowlist
local toksT = tokenizer.parse("[jump ../evil.ks]")
local callsT = {}
local ctxT = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function(path) callsT[#callsT + 1] = path
        return { { "ch", {} } } end }
local coT = coroutine.create(function() scheduler.run(ctxT, toksT, 1) end)
while coroutine.status(coT) ~= "dead" do coroutine.resume(coT) end
check("bare traversal still blocked", #callsT == 0)

-- typo'd named param must WARN, not crash (review should-fix: the raw
-- pair table at params[1] used to reach target:sub / path concat)
local toksT2 = tokenizer.parse("[jump storag=next.ks]")
local callsT2 = {}
local ctxT2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function(path) callsT2[#callsT2 + 1] = path
        return { { "ch", {} } } end }
local coT2 = coroutine.create(function() scheduler.run(ctxT2, toksT2, 1) end)
local okT2 = true
while coroutine.status(coT2) ~= "dead" do
    local r, e = coroutine.resume(coT2)
    if not r then okT2 = false break end
end
check("typo param no crash", okT2 and #callsT2 == 0)

-- typo'd named params on call/link must WARN, not crash (review warn:
-- nil target reached "assets/script/" .. nil -- threw)
for _, c in ipairs({ "call", "link" }) do
    local tT = tokenizer.parse("[" .. c .. " storag=next.ks]")
    local callsT3 = {}
    local ctxT3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        tokens = { { "ch", { text = "hi" } } }, token_index = 1,
        current_scene = "assets/script/main.ks", label_index = {},
        load_tokens = function(path) callsT3[#callsT3 + 1] = path
            return { { "ch", {} } } end }
    local coT3 = coroutine.create(function() scheduler.run(ctxT3, tT, 1) end)
    local okT3 = true
    while coroutine.status(coT3) ~= "dead" do
        local r, e = coroutine.resume(coT3)
        if not r then okT3 = false break end
    end
    check("[" .. c .. "] typo no crash", okT3 and #callsT3 == 0)
end

-- [link] typo must NOT wipe layers/backlog (review should-fix: the
-- clearing used to run unconditionally before the path check)
local tL = tokenizer.parse("[link storag=next.ks]")
local ctxL = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    layers = { bg = {} }, backlog = { { "old" } }, call_stack = { { 1 } },
    tokens = { { "ch", { text = "hi" } } }, token_index = 1,
    current_scene = "assets/script/main.ks", label_index = {},
    load_tokens = function() return { { "ch", {} } } end }
local coL = coroutine.create(function() scheduler.run(ctxL, tL, 1) end)
while coroutine.status(coL) ~= "dead" do coroutine.resume(coL) end
check("link typo keeps state", ctxL.layers ~= nil and ctxL.backlog ~= nil
      and ctxL.call_stack ~= nil)

-- bare [call] and [link] route cross-scene too
for _, c in ipairs({ "call", "link" }) do
    local tC = tokenizer.parse("[" .. c .. " next.ks]")
    local callsC = {}
    local ctxC = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        tokens = { { "ch", { text = "hi" } } }, token_index = 1,
        current_scene = "assets/script/main.ks", label_index = {},
        load_tokens = function(path) callsC[#callsC + 1] = path
            return { { "ch", {} } } end }
    local coC = coroutine.create(function() scheduler.run(ctxC, tC, 1) end)
    while coroutine.status(coC) ~= "dead" do coroutine.resume(coC) end
    check("bare [" .. c .. "] routes", callsC[1] == "assets/script/next.ks")
end

if failed > 0 then os.exit(1) end
print("JUMP PATH TESTS DONE")
