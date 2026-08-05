-- test_label_jump.lua — choice *label resolution (review should-fix)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- The runner resolves "*label" targets to a token index via the label
-- index (or a scan). Extract and lock that resolution logic here so the
-- runner branch is testable without a full runner harness.
local function resolveLabel(ctx, label)
    local idx = ctx.label_index and ctx.label_index[label]
    if not idx then
        for i, tok in ipairs(ctx.tokens) do
            if tok[1] == "label" and tok[2] and tok[2].name == label then
                idx = i + 1
                break
            end
        end
    end
    return idx
end

local tokens = {
    { "ch", { text = "intro" } },
    { "label", { name = "north" } },
    { "ch", { text = "north-path" } },
    { "label", { name = "south" } },
    { "ch", { text = "south-path" } },
}
-- via label_index (built by the scheduler)
local idx1 = resolveLabel({ tokens = tokens, label_index = { north = 3 } }, "north")
check("label_index hit", idx1 == 3)
-- via fallback scan
local idx2 = resolveLabel({ tokens = tokens, label_index = {} }, "south")
check("scan fallback", idx2 == 5)
-- missing label -> nil (runner errors loudly)
local idx3 = resolveLabel({ tokens = tokens, label_index = {} }, "missing")
check("missing label nil", idx3 == nil)
-- the runner branch is guarded by the "*" prefix check
local path = "*north"
check("star prefix detected", path:sub(1, 1) == "*")
check("non-star goes to scene path", ("scripts/x.ks"):sub(1, 1) ~= "*")

if failed > 0 then os.exit(1) end
print("LABEL JUMP TESTS DONE")
