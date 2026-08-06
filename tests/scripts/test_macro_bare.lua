-- test_macro_bare.lua — macro bare-name + numeric placeholders (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local tokenizer = require("tokenizer")
local scheduler = require("scheduler")

-- bare macro name: [macro m ...] -- params[1] = "m" (KAG3 syntax)
local toks = tokenizer.parse("[macro m args=\"1\"][ch text=\"v=%1%\"][endmacro][m 500]")
local dispatched = {}
local kag_orig = package.loaded["kag"]
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched[#dispatched + 1] = { k, p2 } end
end})
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks, token_index = 1, current_scene = "t.ks", label_index = {} }
local co = coroutine.create(function() scheduler.run(ctx, toks, 1) end)
while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
package.loaded["kag"] = kag_orig
local chTexts = {}
for _, d in ipairs(dispatched) do
    if d[1] == "ch" then chTexts[#chTexts + 1] = d[2].text end
end
check("macro body ran", #chTexts >= 1)
check("numeric placeholder filled", chTexts[1] == "v=500")

-- named args still work after the numeric-key change
local toks2 = tokenizer.parse("[macro n args=\"who\"][ch text=\"hi %who%\"][endmacro][n who=Alice]")
local dispatched2 = {}
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched2[#dispatched2 + 1] = { k, p2 } end
end})
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks2, token_index = 1, current_scene = "t.ks", label_index = {} }
local co2 = coroutine.create(function() scheduler.run(ctx2, toks2, 1) end)
while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
package.loaded["kag"] = kag_orig
check("named placeholder filled", dispatched2[1] and dispatched2[1][2].text == "hi Alice")

-- [erasemacro m] bare name
local toks3 = tokenizer.parse("[macro z][ch text=\"z\"][endmacro][erasemacro z][z]")
local dispatched3 = {}
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(c2, p2) dispatched3[#dispatched3 + 1] = { k, p2 } end
end})
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    tokens = toks3, token_index = 1, current_scene = "t.ks", label_index = {} }
local co3 = coroutine.create(function() scheduler.run(ctx3, toks3, 1) end)
while coroutine.status(co3) ~= "dead" do coroutine.resume(co3) end
package.loaded["kag"] = kag_orig
-- after erase, [z] must NOT expand the macro body -- only the bare
-- [z] token itself dispatches (mock __index answers every key, so it
-- shows up as one {cmd="z"} entry; the macro body's ch never runs)
check("erasemacro bare works", #dispatched3 == 1
      and dispatched3[1][1] == "z")

if failed > 0 then os.exit(1) end
print("MACRO BARE TESTS DONE")
