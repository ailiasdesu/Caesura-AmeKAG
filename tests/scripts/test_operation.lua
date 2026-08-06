-- test_operation.lua — Operation cancel semantics (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Operation = require("kag.operation")

-- start registers a token; cancel_all cancels + fires callbacks in
-- reverse registration order
local ctx = {}
local order = {}
local op1 = Operation.start(ctx)
op1.token:register(function() order[#order + 1] = "cb1" end)
local op2 = Operation.start(ctx)
op2.token:register(function() order[#order + 1] = "cb2" end)
check("start registers tokens", #ctx.active_operations == 2)
Operation.cancel_all(ctx)
check("cancel_all clears list", type(ctx.active_operations) == "table"
      and #ctx.active_operations == 0)
check("tokens cancelled", op1.token.cancelled == true
      and op2.token.cancelled == true)
check("callbacks reverse order", order[1] == "cb2" and order[2] == "cb1")

-- cancel_all on a ctx without operations is a no-op
local ctx2 = {}
local ok = pcall(Operation.cancel_all, ctx2)
check("cancel_all nil-safe", ok == true)

-- [wait] cancels cleanly: the wait loop breaks on cancelled and skips
-- complete (source-level: the guard shape)
local f = assert(io.open("scripts/kag/commands/system.lua", "r"))
local src = f:read("*a")
f:close()
check("wait checks cancelled", src:find("not ct.cancelled", 1, true) ~= nil)
check("wait breaks on cancel", src:find("if ct.cancelled then break end", 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("OPERATION TESTS DONE")
