-- test_toast.lua — toast notification lifecycle (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- toast requires backend/layers (render) -- lock the lifecycle via the
-- source contracts (TTL decay, overwrite, expiry hide)
local f = assert(io.open("scripts/toast.lua", "r"))
local src = f:read("*a")
f:close()
check("ttl decay", src:find("state.ttl = state.ttl - (dt or 0.016)", 1, true) ~= nil)
check("expiry hides", src:find("state.ttl <= 0", 1, true) ~= nil
      and src:find("bg.visible = false", 1, true) ~= nil)
check("overwrite", src:find("state.msg = msg", 1, true) ~= nil
      and src:find("state.ttl = state.maxTtl", 1, true) ~= nil)
check("isVisible gate", src:find("state.msg ~= nil and state.ttl > 0", 1, true) ~= nil)
check("ttl default 2s", src:find("ttl or 2.0", 1, true) ~= nil)

-- save/load completion uses toast (feedback contract)
local f2 = assert(io.open("scripts/system.lua", "r"))
local src2 = f2:read("*a")
f2:close()
check("save feedback", src2:find('require("toast").show(ok and "已保存" or "保存失败", 1.5)', 1, true) ~= nil)
check("load feedback", src2:find('require("toast").show(ok ~= false and "已读档" or "读档失败", 1.5)', 1, true) ~= nil)

if failed > 0 then os.exit(1) end
print("TOAST TESTS DONE")
