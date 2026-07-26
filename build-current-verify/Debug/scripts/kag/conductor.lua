-- ⚠ DEPRECATED - superseded by tokenizer.lua (spec 1.1)
-- This module is no longer auto-loaded by kag/init.lua.
-- Old scripts that explicitly require() this module will still work,
-- but new code should use tokenizer.lua + scheduler.lua directly.
-- See docs/ for migration guide.
-- ⚠ WARNING: This module creates its own LOCAL ctx table (lines ~25-49)
-- that is SEPARATE from the C++ GameState ctx (caesura_ctx in Lua registry).
-- Fields like skipMode/autoMode stored in this local ctx will NOT be persisted
-- by the C++ SaveManager. Use the global GameState ctx for all persisted state.
-- ===========================================================================
--  Caesura (AmeKAG) — kag/conductor.lua
--  KAG Conductor: legacy backward-compatibility wrapper.
--  [P1] NOTE: This module is superseded by scheduler.lua (spec [1.2]).
--  New code should use tokenizer.lua + scheduler.lua directly.
--  This wrapper provides the Conductor.execute/resume/status API.
--
--  IMPORTANT: Does NOT overwrite global KAG.* bindings.
--  C++ KAG bindings are the authoritative implementation.
--  All audio/rendering goes through C++ — backend_factory → C++.
-- ===========================================================================

local Conductor = {}
local kag = require("kag")
local flow = require("flow")

-- Internal state
local ctx = nil
local tokens = {}

-- ===========================================================================
--  Create a minimal context for legacy conductor usage
-- ===========================================================================

local function ensureCtx()
    if not ctx then
        -- [R1-FIX] WARNING: This creates a LOCAL ctx table SEPARATE from
        -- the C++ GameState ctx. Fields like skipMode/autoMode will NOT
        -- be persisted by C++ SaveManager. Use the global GameState ctx.
        ctx = {
            tokens = {},
            labelMap = {},
            callStack = {},
            ifSkip = false,
            backlog = {},
            textCursorX = 32,
            textCursorY = 600,
            waiting_input = false,
            _schedulerState = "IDLE",
            skipMode = false,
            autoMode = false,
            readFile = function(path)
                local f = io.open(path, "r")
                if f then
                    local content = f:read("*a")
                    f:close()
                    return content
                end
                return nil
            end,
        }
    end
    return ctx
end

-- ===========================================================================
--  Legacy API: Conductor.execute(commands) / resume() / status()
--  These use internal kag.* functions (local module), NOT global KAG.*
-- ===========================================================================

function Conductor.execute(commands)
    local scheduler = require("scheduler")

    -- Convert legacy command format to script text for tokenizer
    local scriptLines = {}
    for _, cmd in ipairs(commands) do
        if cmd.type == "text" then
            table.insert(scriptLines, cmd.text or "")
        elseif cmd.type == "tag" then
            local line = "[" .. cmd.command
            if cmd.params then
                for k, v in pairs(cmd.params) do
                    if type(k) == "string" then
                        line = line .. " " .. k .. "=" .. tostring(v)
                    end
                end
            end
            line = line .. "]"
            table.insert(scriptLines, line)
        end
    end

    local scriptText = table.concat(scriptLines, "\n")
    local c = ensureCtx()
    scheduler.load_script(c, scriptText)
end

function Conductor.resume()
    local scheduler = require("scheduler")
    local c = ensureCtx()
    if c.waiting_input then
        scheduler.onClick(c)
    end
end

function Conductor.status()
    local scheduler = require("scheduler")
    local c = ensureCtx()
    return scheduler.status(c)
end

return Conductor