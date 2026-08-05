-- test_label_jump.lua — choice *label resolution (review should-fix)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- Lock the REAL resolver used by the runner branch (review warning: a
-- duplicated copy would let a runner regression pass CI).
-- The sandbox wraps _G.require (preloaded-only) and nils loadfile/dofile
-- after test_sandbox, and test_title_entry leaves a 3-key mock in
-- package.preload -- io.open + load (both retained) is the reliable path.
local krf = assert(io.open("scripts/kag_runner.lua", "r"))
local krsrc = krf:read("*a")
krf:close()
local kr_chunk = assert(load(krsrc, "=kag_runner"))
local kag_runner = kr_chunk()
local resolveLabel = kag_runner.resolve_label_index

local tokens = {
    { "ch", { text = "intro" } },
    { "label", { name = "north" } },
    { "ch", { text = "north-path" } },
    { "label", { name = "south" } },
    { "ch", { text = "south-path" } },
}
-- via label_index (built by the scheduler)
local idx1 = resolveLabel({ tokens = tokens, label_index = { north = 2 } }, "north")
check("label_index hit", idx1 == 2)
-- via fallback scan
local idx2 = resolveLabel({ tokens = tokens, label_index = {} }, "south")
check("scan fallback", idx2 == 4)
-- missing label -> nil (runner errors loudly)
local idx3 = resolveLabel({ tokens = tokens, label_index = {} }, "missing")
check("missing label nil", idx3 == nil)
-- the runner branch is guarded by the "*" prefix check
local path = "*north"
check("star prefix detected", path:sub(1, 1) == "*")
check("non-star goes to scene path", ("scripts/x.ks"):sub(1, 1) ~= "*")

if failed > 0 then os.exit(1) end
print("LABEL JUMP TESTS DONE")
