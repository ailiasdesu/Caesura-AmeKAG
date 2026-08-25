-- =============================================================================
--  replay.lua — input recording / playback (Neo-Genesis)
--
--  Records player input events (clicks with coordinates, choices) with
--  timestamps, then replays them to drive the KAG runner automatically.
--  Use cases:
--    * auto-demo mode (attract screens / trailers run the game itself)
--    * regression playback (re-run a scripted path after engine changes)
--    * accessibility (scripted navigation for testing)
--
--  Events are recorded by kag_runner.on_click (via replay.record) and
--  fired by replay.tick() inside kag_runner.update: each stored event
--  carries the elapsed time it happened (ms since recording start); the
--  playback cursor fires due events and calls the runner callbacks.
--
--  Persistence: replay.save(file)/load(file) use a minimal JSON subset
--  (events are {t=ms,type="click",x=?,y=?}) -- no external dependency.
-- =============================================================================

local replay = {}

local state = {
    mode = "off",        -- "off" | "record" | "playback"
    events = {},         -- recorded/fired event list
    cursor = 1,          -- playback cursor
    elapsed = 0,         -- ms since record/playback start
    started = false,
    file = nil,
    clicks_fired = 0,
}

local function json_escape(s)
    s = tostring(s)
    return '"' .. s:gsub("\\", "\\\\"):gsub('"', '\\"')
        :gsub("\n", "\\n"):gsub("\r", "\\r") .. '"'
end

--- replay.set_mode(mode, file?) — "off" | "record" | "playback".
--  Starting playback resets the cursor; starting record clears events.
function replay.set_mode(mode, file)
    assert(mode == "off" or mode == "record" or mode == "playback",
        "mode must be off|record|playback")
    state.mode = mode
    state.file = file
    state.elapsed = 0
    state.started = false
    state.clicks_fired = 0
    if mode == "record" then
        state.events = {}
        state.cursor = 1
    elseif mode == "playback" then
        state.cursor = 1
    end
    return true
end

function replay.get_mode()
    return state.mode
end

--- replay.record(type, x, y) — called by kag_runner.on_click in record
--  mode; also records choice clicks (coordinates included).
function replay.record(evtype, x, y)
    if state.mode ~= "record" then return false end
    if not state.started then
        state.started = true
        state.elapsed = 0
    end
    state.events[#state.events + 1] = {
        t = state.elapsed,
        type = evtype or "click",
        x = x,
        y = y,
    }
    return true
end

--- replay.tick(delta_ms, click_cb) — advance the clock and (playback)
--  fire due events. Record mode also advances the clock so recorded
--  events carry real inter-click timing.
--  click_cb(x, y) is invoked for each due click (the runner wraps
--  on_click and restores mouse coordinates before calling it).
function replay.tick(delta_ms, click_cb)
    if state.mode ~= "playback" and state.mode ~= "record" then return 0 end
    if not state.started then
        state.started = true
        state.elapsed = 0
    end
    state.elapsed = state.elapsed + (tonumber(delta_ms) or 0)
    if state.mode ~= "playback" then return 0 end
    local fired = 0
    while state.cursor <= #state.events do
        local ev = state.events[state.cursor]
        if ev.t > state.elapsed then break end
        state.cursor = state.cursor + 1
        if ev.type == "click" then
            fired = fired + 1
            state.clicks_fired = state.clicks_fired + 1
            if click_cb then
                pcall(click_cb, ev.x, ev.y)
            end
        end
    end
    return fired
end

--- replay.is_done() — playback finished (all events consumed)?
function replay.is_done()
    return state.mode == "playback" and state.cursor > #state.events
end

function replay.event_count()
    return #state.events
end

function replay.clicks_fired()
    return state.clicks_fired
end

--- replay.save(file) — write the recorded events as JSON.
function replay.save(file)
    file = file or state.file
    if not file then return false, "no file" end
    local parts = {}
    for i, ev in ipairs(state.events) do
        parts[i] = string.format("{\"t\":%d,\"type\":%s%s%s}",
            math.floor(ev.t or 0),
            json_escape(ev.type or "click"),
            ev.x ~= nil and (",\"x\":" .. tostring(ev.x)) or "",
            ev.y ~= nil and (",\"y\":" .. tostring(ev.y)) or "")
    end
    local f, err = io.open(file, "w")
    if not f then return false, err end
    f:write("[" .. table.concat(parts, ",") .. "]")
    f:close()
    return true
end

--- replay.load(file) — read events; returns count or nil + error.
function replay.load(file)
    local f, err = io.open(file, "r")
    if not f then return nil, err end
    local text = f:read("*a")
    f:close()
    -- minimal JSON array parser: {"t":123,"type":"click","x":1,"y":2}
    -- (Lua patterns have no non-capturing groups; parse the payload tail
    -- with a second pass for the optional coordinates)
    local events = {}
    for t, evtype, rest in text:gmatch('{"t":(%d+),"type":"([^"]+)"(.-)}') do
        local x = rest:match('"x":([%d%-]+)')
        local y = rest:match('"y":([%d%-]+)')
        events[#events + 1] = {
            t = tonumber(t),
            type = evtype,
            x = x and tonumber(x) or nil,
            y = y and tonumber(y) or nil,
        }
    end
    state.events = events
    state.cursor = 1
    state.elapsed = 0
    state.started = false
    state.clicks_fired = 0
    return #events
end

--- replay.snapshot() — status for the editor / debug HUD.
function replay.snapshot()
    return {
        mode = state.mode,
        events = #state.events,
        cursor = state.cursor,
        elapsed = math.floor(state.elapsed),
        clicks_fired = state.clicks_fired,
    }
end

return replay
