-- =============================================================================
--  Caesura (AmeKAG) — pool.lua
--  Generic object pool for high-churn Lua tables (render_batch, event tables,
--  CancelToken wrappers). Reduces GC pressure per spec [10.2.29].
--
--  Usage:
--    local batchPool = Pool.new(
--        function() return {cmd="", x=0, y=0, w=0, h=0, data={}} end,
--        function(obj) obj.cmd=""; obj.x=0; obj.y=0; obj.w=0; obj.h=0;
--                      for k in pairs(obj.data) do obj.data[k]=nil end end,
--        64  -- max pool size
--    )
--    local b = batchPool:acquire()
--    -- ... use b ...
--    batchPool:release(b)
-- =============================================================================

local Pool = {}

-- Pool metatable with __gc for safety
local PoolMT = { __index = Pool }

--- Create a new object pool.
--- @param ctor   function() → new object
--- @param reset  function(obj) → reset object to clean state
--- @param maxSize number (default 256) max pooled objects
--- @return pool table with :acquire() / :release(obj) / :stats()
function Pool.new(ctor, reset, maxSize)
    maxSize = maxSize or 256
    local pool = {
        _ctor    = ctor,
        _reset   = reset,
        _maxSize = maxSize,
        _free    = {},  -- stack of free objects
        _created = 0,   -- total created (stats)
        _acquired = 0,  -- currently in use (stats)
    }
    return setmetatable(pool, PoolMT)
end

--- Acquire an object from the pool (or create new if empty).
function Pool:acquire()
    local n = #self._free
    if n > 0 then
        local obj = self._free[n]
        self._free[n] = nil
        self._acquired = self._acquired + 1
        return obj
    end
    -- Create new
    self._created = self._created + 1
    self._acquired = self._acquired + 1
    return self._ctor()
end

--- Release an object back to the pool.
function Pool:release(obj)
    if not obj then return end
    self._acquired = self._acquired - 1
    if #self._free >= self._maxSize then
        -- Pool full, let GC collect it
        return
    end
    if self._reset then
        self._reset(obj)
    end
    self._free[#self._free + 1] = obj
end

--- Pre-allocate n objects into the pool.
function Pool:prewarm(n)
    for _ = 1, n do
        local obj = self._ctor()
        self._created = self._created + 1
        self._free[#self._free + 1] = obj
    end
end

--- Return pool statistics.
function Pool:stats()
    return {
        created  = self._created,
        acquired = self._acquired,
        free     = #self._free,
        max      = self._maxSize,
    }
end

--- Drain all free objects (force GC collect).
function Pool:drain()
    for i = 1, #self._free do
        self._free[i] = nil
    end
end

-- =============================================================================
--  Pre-built pools for engine internals
-- =============================================================================

-- Render batch pool (high churn: every layer every frame)
Pool.renderBatchPool = Pool.new(
    function()
        return {
            cmd  = "",
            x    = 0,  y = 0,  w = 0,  h = 0,
            r    = 255, g = 255, b = 255, a = 255,
            data = {},  -- sub-entries for text glyphs, etc.
        }
    end,
    function(b)
        b.cmd = ""
        b.x = 0; b.y = 0; b.w = 0; b.h = 0
        b.r = 255; b.g = 255; b.b = 255; b.a = 255
        -- Clear data table entries but keep the table
        for k in pairs(b.data) do b.data[k] = nil end
    end,
    128  -- max pooled render batches
)

-- Event table pool (flow events, CancelToken callbacks)
Pool.eventTablePool = Pool.new(
    function() return {} end,
    function(t) for k in pairs(t) do t[k] = nil end end,
    64
)

-- Small table pool (temporary tables in hot paths)
Pool.smallTablePool = Pool.new(
    function() return {} end,
    function(t) for k in pairs(t) do t[k] = nil end end,
    256
)

-- Prewarm pools for smoother first-frame performance
Pool.renderBatchPool:prewarm(16)
Pool.eventTablePool:prewarm(8)
Pool.smallTablePool:prewarm(32)

return Pool
