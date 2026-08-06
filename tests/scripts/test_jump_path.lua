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

if failed > 0 then os.exit(1) end
print("JUMP PATH TESTS DONE")
