-- ===========================================================================
--  Caesura (AmeKAG) — scheduler.lua
--  Spec [1.2]: Coroutine-based KAG script scheduler.
--  Traverses token sequence, dispatches cmd to kag[cmd](ctx, params).
--  State machine: IDLE → RUNNING → WAIT_TIME|WAIT_CLICK|AWAIT_SOUND|AWAIT_TRANS → DEAD
--  Single entry point: Scheduler.update(ctx, dt) — called from C++ every frame.
-- ===========================================================================

local kag  = require("kag")
local flow = require("flow")

local Scheduler = {}

-- Back-reference so kag/flow can notify the scheduler
kag.scheduler_ref = Scheduler

-- ===========================================================================
--  Internal: macro expansion
--  Replaces {{macro_name}} in text content with stored macro bodies.
-- ===========================================================================

local function expandMacros(text, macros)
    if not text or not macros then return text end
    return (text:gsub("{{([%w_]+)}}", function(name)
        return macros[name] or ("{{" .. name .. "}}")
    end))
end

-- ===========================================================================
--  Internal: [eval] expression execution
--  Evaluates a Lua expression and returns the result.
--  Supports: [eval exp="1+2"] → result stored, [eval exp="var=5"] → assignment
-- ===========================================================================

local function evalExpression(ctx, exp, store)
    if not exp or #exp == 0 then return nil end
    local fn, err = load("return " .. exp, "=eval")
    if not fn then
        -- Try as a statement (assignment, function call, etc.)
        fn, err = load(exp, "=eval")
    end
    if fn then
        local env = setmetatable({}, { __index = _G })
        if ctx.evalVars then
            setmetatable(env, { __index = function(_, k) return ctx.evalVars[k] or _G[k] end })
        end
        setfenv(fn, env)
        local ok, result = pcall(fn)
        if ok then
            ctx.evalVars = ctx.evalVars or {}
            if store then
                ctx.evalVars[store] = result
            end
            return result
        else
            print("[Scheduler] eval error: " .. tostring(result))
        end
    else
        print("[Scheduler] eval compile error: " .. tostring(err))
    end
    return nil
end

-- ===========================================================================
-- Scheduler.load_script(ctx, script_text [, start_token])
-- Parse script text, build label map, create coroutine.
-- Spec [1.2]: Primary entry point for loading a KAG script.
-- ===========================================================================

function Scheduler.load_script(ctx, script_text, start_token)
    local tokenizer = require("tokenizer")
    local tokens, macros = tokenizer.parse(script_text)

    -- Store macros for expansion during execution
    ctx.macros = macros

    -- Build label map (first pass)
    ctx.labelMap = {}
    for i, tok in ipairs(tokens) do
        if tok.type == "command" and tok.cmd == "label" and tok.params and tok.params.name then
            ctx.labelMap[tok.params.name] = i
        end
    end

    ctx.tokens = tokens
    ctx.callStack = {}
    ctx.ifSkipStack = {}  -- stack for nested [if] blocks
    ctx.ifSkip = false
    ctx.ifHadMatch = false

    -- State tracking per spec
    ctx._schedulerState = "IDLE"

    -- [P0.1] Skip mode: Ctrl-triggered fast-forward
    if ctx.skipMode == nil then ctx.skipMode = false end
    if ctx.skipFrameCounter == nil then ctx.skipFrameCounter = 0 end
    ctx.skipTokensPerFrame = 4

    -- [P0.2] Auto-read mode: auto-advances after N frames
    if ctx.autoMode == nil then ctx.autoMode = false end
    if ctx.autoFrameCounter == nil then ctx.autoFrameCounter = 0 end
    ctx.autoFramesPerClick = 180  -- 3 sec @ 60fps

    ctx.status = "running"
    ctx._startToken = start_token or 1
    ctx.evalVars = ctx.evalVars or {}  -- persistent eval variable store

    -- Create coroutine
    if ctx.co and coroutine.status(ctx.co) ~= "dead" then
        ctx.co = nil
    end

    ctx.co = coroutine.create(function()
        Scheduler._execute(ctx, tokens)
    end)

    -- Store globally for C++ resume
    _G._KAG_COROUTINE = ctx.co

    -- Kick off: resume until first yield
    ctx._schedulerState = "RUNNING"
    local ok, err = coroutine.resume(ctx.co, 0)
    if not ok then
        print("[Scheduler] Boot error at line " .. (ctx.token_ptr or "?") .. ": " .. tostring(err))
        ctx.co, ctx.status, ctx._schedulerState = nil, "dead", "DEAD"
    else
        ctx.status = coroutine.status(ctx.co)
        if ctx.status == "suspended" then
            ctx._schedulerState = ctx.waiting_input and "WAIT_CLICK" or "WAIT_TIME"
        elseif ctx.status == "dead" then
            ctx._schedulerState = "DEAD"
        end
    end
end

-- Legacy alias
Scheduler.boot = Scheduler.load_script

-- ===========================================================================
-- Scheduler.update(ctx, dt) — called every frame.
-- Resumes KAG coroutine, passing dt to yielding wait commands.
-- ===========================================================================

function Scheduler.update(ctx, dt)
    if not ctx.co then
        ctx._schedulerState = "IDLE"
        return
    end

    if coroutine.status(ctx.co) == "dead" then
        ctx.co, ctx.status, ctx._schedulerState = nil, "dead", "DEAD"
        return
    end

    -- [P0.2] Auto-read mode
    if ctx.autoMode and ctx.waiting_input then
        ctx.autoFrameCounter = (ctx.autoFrameCounter or 0) + 1
        if ctx.autoFrameCounter >= (ctx.autoFramesPerClick or 180) then
            ctx.autoFrameCounter = 0
            Scheduler.onClick(ctx)
            return
        end
    end

    -- [P0.1] Skip mode: advance fast, auto-skip blocking commands
    if ctx.skipMode then
        ctx.skipFrameCounter = (ctx.skipFrameCounter or 0) + 1
        if ctx.skipFrameCounter >= ctx.skipTokensPerFrame then
            ctx.skipFrameCounter = 0
            if ctx.waiting_input then
                Scheduler.onClick(ctx)
                return
            end
            if ctx.co and coroutine.status(ctx.co) == "suspended" then
                local ok, err = coroutine.resume(ctx.co, dt * 10) -- 10x speed
                if not ok then
                    print("[Scheduler] Skip error at line " .. (ctx.token_ptr or "?") .. ": " .. tostring(err))
                    ctx.co, ctx.status, ctx._schedulerState = nil, "dead", "DEAD"
                else
                    ctx.status = coroutine.status(ctx.co)
                    if ctx.status == "suspended" then
                        ctx._schedulerState = ctx.waiting_input and "WAIT_CLICK" or "WAIT_TIME"
                    elseif ctx.status == "dead" then
                        ctx._schedulerState = "DEAD"
                    end
                end
            end
            return
        end
    end

    -- Normal resume: pass dt for time-based yields
    if ctx.co and coroutine.status(ctx.co) == "suspended" then
        local ok, err = coroutine.resume(ctx.co, dt)
        if not ok then
            print("[Scheduler] Runtime error at line " .. (ctx.token_ptr or "?") .. ": " .. tostring(err))
            ctx.co, ctx.status, ctx._schedulerState = nil, "dead", "DEAD"
        else
            ctx.status = coroutine.status(ctx.co)
            if ctx.status == "suspended" then
                ctx._schedulerState = ctx.waiting_input and "WAIT_CLICK" or "WAIT_TIME"
            elseif ctx.status == "dead" then
                ctx._schedulerState = "DEAD"
            end
        end
    end
end

-- ===========================================================================
-- Scheduler.onClick(ctx) — user click handler. Unblocks [p] / [l].
-- ===========================================================================

function Scheduler.onClick(ctx)
    if ctx.waiting_input then
        ctx.waiting_input = false
    end
end

-- ===========================================================================
--  Internal: push/pop [if] nesting state
-- ===========================================================================

local function pushIfState(ctx, skip)
    ctx.ifSkipStack = ctx.ifSkipStack or {}
    table.insert(ctx.ifSkipStack, { skip = ctx.ifSkip, hadMatch = ctx.ifHadMatch })
    ctx.ifSkip = skip
    ctx.ifHadMatch = not skip
end

local function popIfState(ctx)
    ctx.ifSkipStack = ctx.ifSkipStack or {}
    if #ctx.ifSkipStack > 0 then
        local prev = table.remove(ctx.ifSkipStack)
        ctx.ifSkip = prev.skip
        ctx.ifHadMatch = prev.hadMatch
    else
        ctx.ifSkip = false
        ctx.ifHadMatch = false
    end
end

-- ===========================================================================
-- Scheduler._execute(ctx, tokens) — main execution loop (runs inside coroutine).
-- Spec [1.2]: Walks token list, dispatches to kag[cmd] or handles inline.
-- Flow commands (if/elif/else/endif, jump/call/return, switch/case) handled here.
-- ===========================================================================

function Scheduler._execute(ctx, tokens)
    local i = ctx._startToken or 1

    while i <= #tokens do
        local tok = tokens[i]
        ctx.line = tok.line
        ctx.token_ptr = i

        if tok.type == "text" then
            -- Expand macros in text content
            local expanded = expandMacros(tok.content, ctx.macros)
            local handler = kag.text
            if handler then handler(ctx, { [1] = expanded, text = expanded }) end

        elseif tok.type == "command" then
            local cmd = tok.cmd

            -- ================================================================
            --  Flow control commands (handled here, not dispatched to kag)
            -- ================================================================

            -- [if] / [if_] — conditional branch
            if cmd == "if" or cmd == "if_" then
                local cond = true
                local params = tok.params or {}
                local exp = params.exp
                if exp then
                    local result = evalExpression(ctx, exp)
                    cond = (result ~= false and result ~= nil)
                end
                pushIfState(ctx, not cond)
                i = i + 1
                goto continue
            end

            -- [elif] / [elif_] — else-if branch
            if cmd == "elif" or cmd == "elif_" then
                local state = ctx.ifSkipStack and ctx.ifSkipStack[#ctx.ifSkipStack]
                if state and state.hadMatch then
                    ctx.ifSkip = true
                elseif state then
                    local cond = true
                    local exp = (tok.params or {}).exp
                    if exp then
                        local result = evalExpression(ctx, exp)
                        cond = (result ~= false and result ~= nil)
                    end
                    ctx.ifSkip = not cond
                    if cond then
                        state.hadMatch = true
                        ctx.ifHadMatch = true
                    end
                end
                i = i + 1
                goto continue
            end

            -- [else] / [else_] — fallback branch
            if cmd == "else" or cmd == "else_" then
                local state = ctx.ifSkipStack and ctx.ifSkipStack[#ctx.ifSkipStack]
                if state then
                    ctx.ifSkip = state.hadMatch
                end
                i = i + 1
                goto continue
            end

            -- [endif] — end of conditional block
            if cmd == "endif" then
                popIfState(ctx)
                i = i + 1
                goto continue
            end

            -- [jump] [call] [return] [link] — flow redirection
            if cmd == "jump" then
                local target = (tok.params or {}).target or (tok.params or {}).storage or (tok.params or {})[1]
                if target and ctx.labelMap[target] then
                    i = ctx.labelMap[target]
                    goto continue
                else
                    print("[Scheduler] jump target not found: " .. tostring(target))
                end
            end

            if cmd == "call" then
                local target = (tok.params or {}).target or (tok.params or {}).storage or (tok.params or {})[1]
                if target and ctx.labelMap[target] then
                    ctx.callStack = ctx.callStack or {}
                    table.insert(ctx.callStack, i + 1)
                    i = ctx.labelMap[target]
                    goto continue
                else
                    print("[Scheduler] call target not found: " .. tostring(target))
                end
            end

            if cmd == "return" or cmd == "return_" then
                if ctx.callStack and #ctx.callStack > 0 then
                    i = table.remove(ctx.callStack)
                    goto continue
                else
                    -- Top-level return → stop
                    ctx._schedulerState = "DEAD"
                    return
                end
            end

            if cmd == "link" then
                local target = (tok.params or {}).target or (tok.params or {}).storage or (tok.params or {})[1]
                if target then
                    -- Load new script, preserving eval vars
                    local newScript = target
                    -- Try loading as file
                    local f = io.open(newScript, "r")
                    if f then
                        local text = f:read("*a")
                        f:close()
                        local tokenizer = require("tokenizer")
                        local newTokens, newMacros = tokenizer.parse(text)
                        tokens = newTokens
                        if newMacros then
                            for k, v in pairs(newMacros) do ctx.macros[k] = v end
                        end
                        -- Rebuild label map
                        ctx.labelMap = {}
                        for j, t in ipairs(tokens) do
                            if t.type == "command" and t.cmd == "label" and t.params and t.params.name then
                                ctx.labelMap[t.params.name] = j
                            end
                        end
                        i = 1
                        goto continue
                    end
                end
            end

            -- [end] — stop script
            if cmd == "end" then
                ctx._schedulerState = "DEAD"
                return
            end

            -- [switch] / [switch_] — switch block
            if cmd == "switch" or cmd == "switch_" then
                local exp = (tok.params or {}).exp or (tok.params or {})[1]
                local switchVal = exp and evalExpression(ctx, exp)
                ctx._switchVal = switchVal
                ctx._switchMatched = false
                i = i + 1
                goto continue
            end

            -- [case] / [case_]
            if cmd == "case" or cmd == "case_" then
                local caseVal = (tok.params or {}).val or (tok.params or {})[1]
                if ctx._switchMatched then
                    ctx.ifSkip = true
                elseif caseVal ~= nil and ctx._switchVal ~= nil then
                    local match = (tostring(caseVal) == tostring(ctx._switchVal))
                    if match then
                        ctx._switchMatched = true
                        ctx.ifSkip = false
                    else
                        ctx.ifSkip = true
                    end
                end
                i = i + 1
                goto continue
            end

            -- [default] / [default_]
            if cmd == "default" or cmd == "default_" then
                ctx.ifSkip = ctx._switchMatched or false
                i = i + 1
                goto continue
            end

            -- [endswitch] / [endswitch_]
            if cmd == "endswitch" or cmd == "endswitch_" then
                ctx._switchVal = nil
                ctx._switchMatched = nil
                ctx.ifSkip = false
                i = i + 1
                goto continue
            end

            -- === [eval] — inline Lua expression evaluation ===
            if cmd == "eval" then
                local exp = (tok.params or {}).exp or (tok.params or {})[1]
                local store = (tok.params or {}).store or (tok.params or {}).result
                evalExpression(ctx, exp, store)
                i = i + 1
                goto continue
            end

            -- === [emb] — embed Lua (same as @ prefix but inline tag form) ===
            if cmd == "emb" then
                local code = (tok.params or {}).exp or (tok.params or {})[1]
                if code and #code > 0 then
                    local fn, err = load(code, "=emb")
                    if fn then
                        local ok, runErr = pcall(fn)
                        if not ok then
                            print("[Scheduler] emb error: " .. tostring(runErr))
                        end
                    else
                        print("[Scheduler] emb compile error: " .. tostring(err))
                    end
                end
                i = i + 1
                goto continue
            end

            -- === Skip if inside inactive [if] or [switch] branch ===
            if ctx.ifSkip then
                i = i + 1
                goto continue
            end

            -- === [label]: no-op (pre-resolved in load_script) ===
            if cmd == "label" then
                i = i + 1
                goto continue
            end

            -- === [macro] / [endmacro]: handled by kag (no-op for scheduler) ===
            if cmd == "macro" or cmd == "endmacro" then
                local handler = kag[cmd]
                if handler then handler(ctx, tok.params or {}) end
                i = i + 1
                goto continue
            end

            -- === [erasemacro] — delete macro definition ===
            if cmd == "erasemacro" then
                local name = (tok.params or {}).name or (tok.params or {})[1]
                if name and ctx.macros then ctx.macros[name] = nil end
                i = i + 1
                goto continue
            end

            -- ================================================================
            --  Dispatch ALL other commands to kag[cmd]
            -- ================================================================
            local handler = kag[cmd]
            if handler then
                local result, jump_info = handler(ctx, tok.params or {})

                -- Update state after command execution
                if ctx.waiting_input then
                    ctx._schedulerState = "WAIT_CLICK"
                end

                if result == "stop" then
                    ctx._schedulerState = "DEAD"
                    return
                end

                if result == "jump" and jump_info then
                    i = jump_info.resume_at
                    if jump_info.tokens then
                        tokens = jump_info.tokens
                    end
                    if jump_info.labelMap then
                        ctx.labelMap = jump_info.labelMap
                    end
                    ctx._schedulerState = "JUMP"
                    goto continue
                end

                if result == "return" then
                    if ctx.callStack and #ctx.callStack > 0 then
                        i = table.remove(ctx.callStack)
                        goto continue
                    end
                    ctx._schedulerState = "DEAD"
                    return
                end
            else
                print("[Scheduler] Unknown command: [" .. cmd .. "] at line " .. (tok.line or i))
            end

        elseif tok.type == "embed_lua" then
            -- @-prefixed embedded Lua code
            if tok.lua_code and #tok.lua_code > 0 then
                local fn, err = load(tok.lua_code, "=embed_line_" .. (tok.line or i))
                if fn then
                    local ok, runErr = pcall(fn)
                    if not ok then
                        print("[Scheduler] embed_lua error at line " .. (tok.line or i) .. ": " .. tostring(runErr))
                    end
                else
                    print("[Scheduler] embed_lua compile error at line " .. (tok.line or i) .. ": " .. tostring(err))
                end
            end

        elseif tok.type == "macro_def" then
            -- Already collected in ctx.macros during load_script; no-op here.
        end

        i = i + 1
        ::continue::
    end

    ctx._schedulerState = "DEAD"
    print("[Scheduler] Execution complete. Final token: " .. tostring(ctx.token_ptr))
end

-- ===========================================================================
-- Scheduler.status(ctx) → "running" | "suspended" | "dead" | "none"
-- ===========================================================================

function Scheduler.status(ctx)
    if not ctx.co then return "none" end
    return coroutine.status(ctx.co)
end

-- ===========================================================================
-- Scheduler.schedulerState(ctx) → state string
-- ===========================================================================

function Scheduler.schedulerState(ctx)
    return ctx._schedulerState or "IDLE"
end

-- ===========================================================================
--  Skip / Auto / SaveSkip modes  [P0.1] [P0.2]
-- ===========================================================================

function Scheduler.toggleSkip(ctx)
    ctx.skipMode = not ctx.skipMode
    ctx.skipFrameCounter = 0
    if ctx.skipMode then ctx.autoMode = false end
    print("[Scheduler] Skip mode: " .. tostring(ctx.skipMode))
    return ctx.skipMode
end

function Scheduler.toggleAuto(ctx, speed)
    ctx.autoMode = not ctx.autoMode
    ctx.autoFrameCounter = 0
    if speed and tonumber(speed) then
        ctx.autoFramesPerClick = tonumber(speed)
    end
    if ctx.autoMode then ctx.skipMode = false end
    print("[Scheduler] Auto-read mode: " .. tostring(ctx.autoMode))
    return ctx.autoMode
end

function Scheduler.setAutoSpeed(ctx, frames)
    ctx.autoFramesPerClick = tonumber(frames) or 180
end

return Scheduler