-- test_macro.lua — parameterized macro substitution (Neo-Genesis)
local results = {}  -- file scope: runner shares globals
local function check(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    results[#results + 1] = cond
end

-- drive the scheduler token stream directly
local scheduler = require("scheduler")

-- build a token stream: macro def with args, then two invocations
local tokens = {
    { "macro", { name = "say_hi", args = "who,what" } },
    { "ch", { name = "%who%", text = "%what%" } },
    { "endmacro" },
    { "say_hi", { who = "Sakura", what = "Hello!" } },
    { "say_hi", { who = "Kaito", what = "Yo!" } },
}

-- capture dispatched commands by stubbing the kag table
local kag_orig = package.loaded["kag"]
local dispatched = {}
package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
    return function(ctx, params)
        dispatched[#dispatched + 1] = { k, params }
    end
end})

local ctx = {
    macros = nil, macro_args = nil, f = {},
    current_scene = "test.ks", token_index = 1,
}
scheduler.run(tokens, ctx)

package.loaded["kag"] = kag_orig

check("two dispatches", #dispatched == 2)
check("first substituted name", dispatched[1] and dispatched[1][2].name == "Sakura")
check("first substituted text", dispatched[1] and dispatched[1][2].text == "Hello!")
check("second invocation independent", dispatched[2] and dispatched[2][2].name == "Kaito")
check("second text independent", dispatched[2] and dispatched[2][2].text == "Yo!")
check("shared body not polluted",
      ctx.macros and ctx.macros.say_hi and ctx.macros.say_hi[1][2] == "%who%")

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("MACRO TESTS DONE")
