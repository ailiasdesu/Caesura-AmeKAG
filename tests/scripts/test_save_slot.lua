-- test_save_slot.lua — [save]/[load] bare-slot contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")
require("layers").init() -- slot routing has no visible scene or active VFX

local function run(tokens, overrides)
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
        tokens = tokens, token_index = 1, current_scene = "t.ks",
        label_index = {}, _whileIterByScene = { ["t.ks"] = 0 } }
    for k, v in pairs(overrides or {}) do ctx[k] = v end
    local co = coroutine.create(function() scheduler.run(ctx, tokens, 1) end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    return ctx
end

-- bare [save 3] routes slot 3
local calls = {}
_G.KAG = { save_game = function(slot) calls[#calls + 1] = slot return true end,
           load_game = function() return {}, {} end }
run(tokenizer.parse("[save 3]"))
check("bare slot 3", calls[1] == 3)

-- clamp: 999 -> 99 (numeric key bypasses schema; handler clamps)
calls = {}
run(tokenizer.parse("[save 999]"))
check("clamped 99", calls[1] == 99)

-- negative -> passed through (SYSTEM slots -1/-2 must not map to 0:
-- F5/autosave would overwrite the manual slot -- review warn)
calls = {}
run(tokenizer.parse("[save -5]"))
check("negative passthrough", calls[1] == -5)

-- no arg -> 0
calls = {}
run(tokenizer.parse("[save]"))
check("default 0", calls[1] == 0)

-- named wins over bare
calls = {}
run(tokenizer.parse("[save slot=7]"))
check("named slot wins", calls[1] == 7)

-- [load 2] routes slot 2
local loadCalls = {}
_G.KAG = { load_game = function(slot) loadCalls[#loadCalls + 1] = slot
    return { f = {} }, {} end }
run(tokenizer.parse("[load 2]"))
check("load bare slot", loadCalls[1] == 2)

-- NAMED negative [save slot=-1] flows through (schema min=-2) to the
-- C++ guard -- never clamps to 0 (review should-fix)
calls = {}
_G.KAG = { save_game = function(slot) calls[#calls + 1] = slot return true end,
           load_game = function() return {}, {} end }
run(tokenizer.parse("[save slot=-1]"))
check("named negative passthrough", calls[1] == -1)

-- direct quicksave-style call (bypasses schema) also passes through
calls = {}
local Save = require("kag.commands.save")
local ctxD = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "t.ks", token_index = 1 }
pcall(Save.save, ctxD, { slot = -2, desc = "Autosave" })
check("autosave slot passthrough", calls[1] == -2)

if failed > 0 then os.exit(1) end
print("SAVE SLOT TESTS DONE")
