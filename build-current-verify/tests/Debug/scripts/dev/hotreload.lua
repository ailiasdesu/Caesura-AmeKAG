-- ===========================================================================
--  Caesura (AmeKAG) — hotreload.lua
--  Phase 8.1: Lua-side hot reload helpers.
--  Provides cancel_all_active for the C++ HotReload to call.
-- ===========================================================================

local hotreload = {}

-- Cancel all active operations tracked in ctx.active_operations.
-- Each entry should be a CancelToken table with :cancel().
-- Returns the number of operations cancelled.
function hotreload.cancel_all_active()
    local ctx = require("kag.ctx")
    if not ctx or not ctx.active_operations then
        return 0
    end

    local count = 0
    for _, op in ipairs(ctx.active_operations) do
        if type(op) == "table" and op.mark_cancelled then
            op:mark_cancelled()
            count = count + 1
        end
    end

    -- Brief pause for cancellations to propagate, then execute callbacks
    -- (In practice the caller uses coroutine.yield)
    for _, op in ipairs(ctx.active_operations) do
        if type(op) == "table" and op.execute_callbacks then
            pcall(function() op:execute_callbacks() end)
        end
    end

    -- Clear the list
    ctx.active_operations = {}

    return count
end

return hotreload