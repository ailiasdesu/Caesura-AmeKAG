-- =============================================================================
--  kag_debug.lua — KAG scene-level debugger (Neo-Genesis)
--
--  Breakpoints on scene+command or scene+line, single-step, and variable
--  scope inspection for KAG scripts. The Lua debugger (DebugProtocol) sits
--  BELOW this layer and cannot see KAG tokens; KAG authors debug script
--  flow (which [ch] is next, what f.* holds) through this API.
--
--  Usage from the editor (RPC eval): KAGDebug.set_breakpoint("s.ks", "ch")
--  or KAGDebug.set_breakpoint("s.ks", 42); the scheduler pauses before the
--  matching token and yields "__kag_pause"; the runner stops advancing
--  until KAGDebug.continue_run() / KAGDebug.step().
--
--  Breakpoints are matched against ctx.current_scene (the path as the
--  scheduler knows it, e.g. "assets/script/main.ks") — prefix matching is
--  NOT applied, so the exact scene string must be used.
-- =============================================================================

local kag_debug = {}

local state = {
    enabled = false,
    breakpoints = {},   -- ["scene.ks:ch"] / ["scene.ks:12"] -> true
    step_next = false,
    paused = false,     -- mirror for RPC inspection
}

--- kag_debug.enable(on) — master switch (allows breakpoints without one).
function kag_debug.enable(on)
    state.enabled = on ~= false
end

function kag_debug.is_enabled()
    return state.enabled
end

--- set_breakpoint(scene, cmdOrLine) — cmd name (string) or 1-based line.
--  Enables the debugger implicitly (a set breakpoint that never fires is
--  a silent no-op otherwise).
function kag_debug.set_breakpoint(scene, cmdOrLine)
    assert(type(scene) == "string" and #scene > 0, "scene (string) required")
    assert(type(cmdOrLine) == "string" or type(cmdOrLine) == "number",
        "cmd (string) or line (number) required")
    state.breakpoints[scene .. ":" .. tostring(cmdOrLine)] = true
    state.enabled = true
    return true
end

--- remove_breakpoint(scene, cmdOrLine)
function kag_debug.remove_breakpoint(scene, cmdOrLine)
    state.breakpoints[scene .. ":" .. tostring(cmdOrLine)] = nil
end

--- clear_breakpoints(scene?) — all, or only those of one scene.
function kag_debug.clear_breakpoints(scene)
    if scene then
        local prefix = scene .. ":"
        for k in pairs(state.breakpoints) do
            if k:sub(1, #prefix) == prefix then state.breakpoints[k] = nil end
        end
    else
        state.breakpoints = {}
    end
end

function kag_debug.get_breakpoints()
    return state.breakpoints
end

--- check(ctx, cmd, index) — scheduler hook: returns "pause" when the
--  current token hits a breakpoint or a pending single-step.
function kag_debug.check(ctx, cmd, index)
    if not state.enabled then return nil end
    local scene = ctx.current_scene or ctx.currentScene or "?"
    if state.breakpoints[scene .. ":" .. tostring(cmd)]
        or state.breakpoints[scene .. ":" .. tostring(index)] then
        return "pause"
    end
    if state.step_next then
        state.step_next = false
        return "pause"
    end
    return nil
end

--- step() — pause at the NEXT token (armed before resuming).
function kag_debug.step()
    state.step_next = true
    state.enabled = true
end

--- continue_run() — clear the paused mirror; the runner resumes naturally.
function kag_debug.continue_run()
    state.paused = false
end

function kag_debug.set_paused(v)
    state.paused = v == true
end

function kag_debug.is_paused()
    return state.paused
end

--- inspect(ctx, scope) — serialize KAG variable scopes for the editor:
--  "f" | "sf" | "tf" | "mp" | "lf" | "all" (default "all").
--  Tables are flattened to depth 3 (cycles/over-deep values become "...").
function kag_debug.inspect(ctx, scope)
    local scopes = {
        f = ctx.f, sf = ctx.sf, tf = ctx.tf, mp = ctx.mp, lf = ctx.lf,
    }
    local function serialize(v, depth)
        if type(v) ~= "table" then return v end
        if depth > 3 then return "..." end
        local t = {}
        for k, val in pairs(v) do
            t[tostring(k)] = serialize(val, depth + 1)
        end
        return t
    end
    local out = {}
    if scope == nil or scope == "all" then
        for name, t in pairs(scopes) do out[name] = serialize(t, 0) end
    else
        out[scope] = serialize(scopes[scope], 0)
    end
    return out
end

--- Summary of the current execution position (for editor status panels).
function kag_debug.snapshot(ctx)
    return {
        scene = ctx.current_scene or ctx.currentScene or "?",
        index = ctx.token_index or ctx.tokenIndex or 0,
        paused = state.paused,
        enabled = state.enabled,
        breakpoints = state.breakpoints,
    }
end

--- serialize_json(ctx, scope) — JSON text of the requested scope(s) for
--  the RPC channel (no external JSON dependency; depth-capped).
function kag_debug.serialize_json(ctx, scope)
    local function esc(s)
        s = tostring(s)
        s = s:gsub("\\", "\\\\"):gsub('"', '\\"'):gsub("\n", "\\n")
        return '"' .. s .. '"'
    end
    local function ser(v, depth)
        local tv = type(v)
        if tv == "nil" then return "null" end
        if tv == "boolean" then return v and "true" or "false" end
        if tv == "number" then return tostring(v) end
        if tv == "string" then return esc(v) end
        if tv ~= "table" then return esc(tostring(v)) end
        if depth > 4 then return '"..."' end
        local parts, n = {}, 0
        for k, val in pairs(v) do
            n = n + 1
            parts[n] = ser(k, depth + 1) .. ":" .. ser(val, depth + 1)
        end
        return "{" .. table.concat(parts, ",") .. "}"
    end
    local scopes = {
        f = ctx.f, sf = ctx.sf, tf = ctx.tf, mp = ctx.mp, lf = ctx.lf,
    }
    local out = {}
    if scope == nil or scope == "" or scope == "all" then
        for name, t in pairs(scopes) do out[#out + 1] = esc(name) .. ":" .. ser(t, 0) end
    else
        out[#out + 1] = esc(scope) .. ":" .. ser(scopes[scope], 0)
    end
    return "{" .. table.concat(out, ",") .. "}"
end

return kag_debug
