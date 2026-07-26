-- Shared lifecycle for cancellable KAG operations.

local CancelToken = require("kag.cancel_token")

local Operation = {}
local Scope = {}
Scope.__index = Scope

local function remove_token(ctx, token)
    local active = ctx and ctx.active_operations
    if not active then return end

    for i = #active, 1, -1 do
        if active[i] == token then
            table.remove(active, i)
            return
        end
    end
end

function Scope:complete()
    if not self.token.cancelled then
        self.completed = true
    end
end

function Scope:cancel()
    self.token:cancel()
end

function Scope:__close(error_value)
    if self.closed then return end
    self.closed = true

    if error_value ~= nil or not self.completed then
        self.token:mark_cancelled()
    end
    remove_token(self.ctx, self.token)
    self.token:complete()
end

function Operation.start(ctx)
    assert(type(ctx) == "table", "operation context must be a table")

    local token = CancelToken.new()
    ctx.active_operations = ctx.active_operations or {}
    table.insert(ctx.active_operations, token)

    return setmetatable({
        ctx = ctx,
        token = token,
        completed = false,
        closed = false,
    }, Scope)
end

function Operation.cancel_all(ctx)
    if type(ctx) ~= "table" or type(ctx.active_operations) ~= "table" then
        return
    end

    local active = ctx.active_operations
    ctx.active_operations = {}

    for _, token in ipairs(active) do
        token:mark_cancelled()
    end
    for i = #active, 1, -1 do
        active[i]:execute_callbacks()
    end
end

return Operation
