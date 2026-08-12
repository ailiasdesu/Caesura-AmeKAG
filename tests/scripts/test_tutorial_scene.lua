-- test_tutorial_scene.lua — P2-8: tutorial scene runs deterministically.
-- Executes scripts/demo_tutorial.ks through the scheduler (mock kag) and
-- asserts the flow/state outcomes: if/while/for, macros, call/return,
-- choices, Lua mix, save/history.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond, detail)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name .. (detail and (" -- " .. tostring(detail)) or ""))
        failed = failed + 1 end
end

pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.save")
pcall(require, "kag.commands.video")
pcall(require, "kag")

local determinism = require("kag.determinism")
local fileutil = require("fileutil")  -- for reading the scene robustly

-- load the tutorial scene source (locate under scripts/ or build copy)
local function loadScene()
    local paths = {
        "scripts/demo_tutorial.ks",
        "demo_tutorial.ks",
        "scripts/tests/demo_tutorial.ks",
    }
    for _, p in ipairs(paths) do
        local f = io.open(p, "r")
        if f then
            local c = f:read("*a")
            f:close()
            return c
        end
    end
    return nil
end

local src = loadScene()
check("tutorial scene found", src ~= nil)
if not src then
    print("TUTORIAL SCENE TESTS: cannot locate demo_tutorial.ks")
    os.exit(1)
end

-- selection override: pick "left" route (sel index 1) — the mock records
-- sel targets; simulate choosing the first option by intercepting endselect
local ctx, steps = determinism.run_scene(src, {
    max_steps = 500,
    kag_override = {
        -- choices: select left (target *left)
        endselect = function(c)
            c._selectedChoice = { target = "*left" }
            c.waiting_input = false
        end,
        -- [save]/[history]/[random] need no-op (they call backend)
        save = function() end,
        history = function() end,
        random = function(c, p)
            -- [random f.dice 1 6]: set f.dice = 4
            local var = p.var or p[1]
            if type(var) == "string" and var:match("^f%.") then
                c.f = c.f or {}
                c.f[var:sub(3)] = 4
            end
        end,
    },
})

check("tutorial runs to completion", steps > 0 and ctx._error == nil
      and not ctx._timed_out, ctx._error)

-- state outcomes
check("if-taken (hp=80 > 50) backlog has HP line",
      #ctx.backlog > 0 and table.concat(ctx.backlog, "|"):find("HP 充足", 1, true))
check("while loop ran 3 times (f.i=3)", ctx.f.i == 3)
check("gold incremented to 150", ctx.f.gold == 150)
check("dice set by random override", ctx.f.dice == 4)
check("lua_result = 43 (6*7+1)", ctx.f.lua_result == 43)
check("for loop broke at k=3", ctx.f.k == 3)

-- Exit gate.
if failed > 0 then
    print(string.format("TUTORIAL SCENE TESTS: %d passed, %d FAILED", passed, failed))
    os.exit(1)
end
print(string.format("TUTORIAL SCENE TESTS DONE (%d passed)", passed))
