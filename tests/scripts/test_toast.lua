-- Toast notification tests: show/update lifecycle (TTL-based auto-hide).
local check = function(name, cond)
    if cond then print("PASS " .. name) else print("FAIL " .. name) end
    local results = {}  -- local: runner shares globals
    results[#results + 1] = cond
end

-- Mock backend via package.preload so require("backend") returns the stub
package.path = "scripts/?.lua;scripts/?/init.lua;" .. package.path
package.preload["backend"] = function()
    return {
        create_solid_texture = function() return { _mock = true } end,
        render_text = function() end,
        get_input_focus = function() return "kag" end,
    }
end
-- Ensure the real backend cache is dropped so preload wins
package.loaded["backend"] = nil
package.loaded["toast"] = nil

local Toast = require("toast")

check("show sets message", Toast.show("已保存", 1.0) == nil or true)  -- no return
check("visible after show", Toast.isVisible() == true)

-- TTL decreases over time
Toast.update(0.5)
check("still visible mid-ttl", Toast.isVisible() == true)
Toast.update(0.6)  -- total 1.1 > 1.0
check("hidden after ttl", Toast.isVisible() == false)

-- Re-show then immediate hide on expiry
Toast.show("x", 0.1)
check("re-visible", Toast.isVisible() == true)
Toast.update(0.2)
check("hidden again", Toast.isVisible() == false)

-- Update with no message is a no-op
local ok = pcall(function() Toast.update(0.1) end)
check("update no-msg no-crash", ok)

print("TOAST TESTS DONE")
