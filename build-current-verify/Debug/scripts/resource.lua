-- ===========================================================================
--  Caesura (AmeKAG) -- resource.lua
--  Resource lifecycle management per spec [0.2].
--  All GPU textures, audio handles, and RTT canvases are tracked here.
--  Lua GC triggers automatic C++ resource release via __gc metamethod.
-- ===========================================================================

local backend = require("backend")

local Resource = {}
Resource.__index = Resource

-- Active resource registry: {handle_id = resource_object}
Resource._registry = {}

-- Pool counters
Resource._nextId = 1

function Resource.new(resourceType, handleId, cppOwner)
    local obj = setmetatable({
        _type     = resourceType,  -- "tex", "audio", "rtt", "rule"
        _id       = handleId or Resource._nextId,
        _alive    = true,
        _cppOwner = cppOwner,      -- which C++ pool manages this
    }, Resource)

    if not handleId then
        Resource._nextId = Resource._nextId + 1
    end

    Resource._registry[obj._id] = obj
    return obj
end

function Resource:destroy()
    if not self._alive then return end
    self._alive = false
    Resource._registry[self._id] = nil

    if self._type == "tex" then
        if self._id then backend.destroy_texture(self._id) end
    elseif self._type == "rtt" then
        if self._id then backend.destroy_viewport(self._id) end
    elseif self._type == "audio" then
        -- Flush SE handles + wave cache in C++ to stop orphaned sound effects
        backend.stop_se()
        backend.flush_wave_cache()
    end
end

-- __gc: called by Lua GC when object becomes unreachable
function Resource:__gc()
    if self._alive then
        print(string.format("[Resource WARN] %s id=%d leaked to GC, auto-releasing",
            self._type, self._id))
        self:destroy()
    end
end

-- Flush all resources owned by a specific C++ pool
function Resource.flushByPool(cppOwner)
    for id, obj in pairs(Resource._registry) do
        if obj._cppOwner == cppOwner then
            obj:destroy()
        end
    end
end

-- Get active resource count
function Resource.activeCount()
    local n = 0
    for _ in pairs(Resource._registry) do n = n + 1 end
    return n
end

return Resource