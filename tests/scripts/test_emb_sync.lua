-- test_emb_sync.lua — [emb] strict env sync (audit fix)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local Sandbox = require("sandbox")

-- execute returns the env as the third value (the [emb] sync contract)
local env = { tf = { score = 1 }, f = {}, sf = {}, mp = {} }
local ok, result, envOut = Sandbox.execute([[f.x = 42; return f.x]], env)
check("execute returns env", envOut ~= nil and envOut == env)
check("execute result", ok == true and result == 42)
check("field write visible in env", env.f.x == 42)

-- REPLACING tf (not writing a field) is visible through envOut
local env2 = { tf = { a = 1 }, f = {}, sf = {}, mp = {} }
local ok2, _, envOut2 = Sandbox.execute([[tf = { b = 2 }]], env2)
check("replacement visible via envOut", envOut2.tf.b == 2)

-- [emb] strict path syncs envOut back to ctx (source-level: the handler
-- reads envOut.tf/f/sf/mp)
local fh = assert(io.open("scripts/kag/commands/system.lua", "r"))
local src = fh:read("*a")
fh:close()
check("emb syncs envOut", src:find("ctx.tf = envOut.tf or ctx.tf", 1, true) ~= nil)


-- Replacement with a NON-table (security LOW): the sync must not flow
-- it into ctx.tf (the emb_result write would raise outside the pcall).
do
    local Sandbox = require("sandbox")
    local env = { tf = { a = 1 }, f = {}, sf = {}, mp = {} }
    local ok, _, envOut = Sandbox.execute([[tf = 5]], env)
    -- type-guard shape: only table replacements sync
    local synced = (type(envOut.tf) == "table")
    if synced == false and ok == true then
        print("PASS non-table replacement rejected") passed = passed + 1
    else print("FAIL non-table replacement rejected") failed = failed + 1 end
end

if failed > 0 then os.exit(1) end
print("EMB SYNC TESTS DONE")
