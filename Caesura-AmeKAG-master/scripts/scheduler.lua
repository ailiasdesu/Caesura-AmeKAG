-- =============================================================================
--  Caesura (AmeKAG) — scheduler.lua
--  Token stream executor. Iterates tokens, dispatches to kag[cmd](ctx, params),
--  handles flow-control inline (if/jump/call/return/label/end/macro/eval/wait).
--  Coroutine-based: yields on blocking ops, resumes next frame from token_index.
-- =============================================================================

local scheduler = {}

-- ── Flow-control command set (handled inline, never dispatched to kag table) ──

local flow_commands = {
    ["if"] = true, ["else"] = true, ["endif"] = true,
    ["switch"] = true, ["case"] = true, ["endswitch"] = true,
    ["jump"] = true, ["call"] = true, ["return"] = true,
    ["link"] = true, ["end"] = true,
    ["label"] = true,
    ["macro"] = true, ["endmacro"] = true, ["erasemacro"] = true,
    ["eval"] = true, ["emb"] = true,
    ["stop"] = true,
}

-- ── Internal helpers ────────────────────────────────────────────────────────

local function find_label(tokens, name)
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and tok[2] and tok[2].name == name then
            return i
        end
    end
    return nil
end

local function skip_to(tokens, start_idx, targets)
    local depth = 1
    for i = start_idx + 1, #tokens do
        local cmd = tokens[i][1]
        if targets[cmd] then
            if depth == 1 then return i end
            depth = depth - 1
        elseif targets.opens and targets.opens[cmd] then
            depth = depth + 1
        end
    end
    return #tokens
end

-- ── Main execution loop ─────────────────────────────────────────────────────

function scheduler.run(ctx, tokens, start_index)
    if not tokens or #tokens == 0 then return end
    local kag = require("kag")
    local cancel_token = require("kag.cancel_token")
    start_index = start_index or 1

    local i = start_index
    while i <= #tokens do
        local tok = tokens[i]
        local cmd = tok[1]
        local params = tok[2] or {}

        -- Check stop flag
        if ctx.stop_flag then return end

        -- Flow control: [jump]
        if cmd == "jump" then
            local target = params.target or params.label or params.storage
            if params.target then
                -- Cross-scene jump: load new scene
                local path = "assets/script/" .. target
                local new_tokens = ctx.load_tokens and ctx.load_tokens(path)
                if new_tokens then
                    ctx.tokens = new_tokens
                    ctx.token_index = 1
                    ctx.current_scene = path
                    ctx.call_stack = {}
                    ctx.layers = {}
                    ctx.backlog = {}
                    -- Cancel all active operations
                    for _, ct in ipairs(ctx.active_operations or {}) do
                        ct:mark_cancelled()
                    end
                    ctx.active_operations = {}
                    return
                end
            else
                -- Intra-scene jump: find label
                local idx = find_label(tokens, target)
                if idx then i = idx end
            end

        -- Flow control: [call]
        elseif cmd == "call" then
            local target = params.target or params.storage
            table.insert(ctx.call_stack, {tokens = tokens, index = i + 1})
            local path = "assets/script/" .. target
            local new_tokens = ctx.load_tokens and ctx.load_tokens(path)
            if new_tokens then
                tokens = new_tokens
                ctx.tokens = tokens
                ctx.current_scene = path
                i = 0
            end

        -- Flow control: [return]
        elseif cmd == "return" then
            local frame = table.remove(ctx.call_stack)
            if frame then
                tokens = frame.tokens
                ctx.tokens = tokens
                i = frame.index - 1
            else
                return  -- No call stack, end execution
            end

        -- Flow control: [link]
        elseif cmd == "link" then
            local target = params.target or params.storage
            -- Clear everything and jump
            ctx.layers = {}
            ctx.backlog = {}
            for _, ct in ipairs(ctx.active_operations or {}) do
                ct:mark_cancelled()
            end
            ctx.active_operations = {}
            ctx.call_stack = {}
            local path = "assets/script/" .. target
            local new_tokens = ctx.load_tokens and ctx.load_tokens(path)
            if new_tokens then
                tokens = new_tokens
                ctx.tokens = tokens
                ctx.token_index = 1
                ctx.current_scene = path
                i = 0
            end

        -- Flow control: [end]
        elseif cmd == "end" then
            return

        -- Flow control: [label] — no-op, used by jump/call
        elseif cmd == "label" then
            -- pass

        -- Flow control: [if]/[else]/[endif]
        elseif cmd == "if" then
            local expr = params.exp or "false"
            local ok, result = pcall(function()
                local fn = load("return " .. expr, "=if", "t", ctx.f or {})
                return fn()
            end)
            if not (ok and result) then
                i = skip_to(tokens, i, {
                    ["else"] = true, ["endif"] = true,
                    opens = {["if"] = true}
                })
            end

        elseif cmd == "else" then
            i = skip_to(tokens, i, {
                ["endif"] = true,
                opens = {["if"] = true}
            })

        elseif cmd == "endif" then
            -- pass

        -- Flow control: [switch]/[case]/[default]/[endswitch] (spec [1.4])
        elseif cmd == "switch" then
            local expr = params.exp or ""
            -- Resolve expression
            local ok, expr_val = pcall(function()
                local fn = load("return " .. expr, "=switch", "t", ctx.f or {})
                return fn()
            end)
            local target = ok and tostring(expr_val) or ""
            -- Phase 1: scan to find the matching case body range
            local body_start = 0
            local body_end   = 0
            local depth = 0
            local found_default = 0
            for j = i + 1, #tokens do
                local tc = tokens[j][1]
                local tp = tokens[j][2] or {}
                if tc == "case" then
                    if body_start == 0 then
                        if tp.value == target then
                            body_start = j + 1
                        end
                    end
                elseif tc == "default" then
                    if body_start == 0 then
                        found_default = j + 1
                        body_start = found_default  -- fallback
                    end
                elseif tc == "switch" then
                    depth = depth + 1
                elseif tc == "endswitch" then
                    if depth == 0 then
                        body_end = j
                        break
                    end
                    depth = depth - 1
                end
            end
            -- Phase 2: execute matched case body by dispatching non-flow tokens
            if body_start > 0 and body_end > body_start then
                for j = body_start, body_end - 1 do
                    local tj = tokens[j]
                    local tcmd = tj[1]
                    local tparams = tj[2] or {}
                    if not flow_commands[tcmd] then
                        local handler = kag[tcmd]
                        if not handler and type(tcmd) == "string" and #tcmd > 0 then
                            handler = kag["ch"]
                            if handler then tparams = {text = tcmd}; tcmd = "ch" end
                        end
                        if handler then
                            local st, er = pcall(handler, ctx, tparams)
                            if not st then
                                print("[ERROR] KAG switch body '" .. tcmd .. "': " .. tostring(er))
                                if ctx.handle_error then ctx.handle_error(tcmd, tostring(er), j) end
                            end
                        end
                    end
                    ctx.token_index = j
                    coroutine.yield()
                end
            end
            i = body_end  -- skip past [endswitch]; loop increment advances to next token

        elseif cmd == "case" or cmd == "default" or cmd == "endswitch" then
            -- pass (handled by [switch] above)

        -- Flow control: [eval]
        elseif cmd == "eval" then
            local code = params.exp or params.code or ""
            local fn = load(code, "=eval", "t", ctx.f or {})
            if fn then pcall(fn) end

        -- Flow control: [macro] / [endmacro]
        elseif cmd == "macro" then
            local name = params.name
            -- Collect macro body until [endmacro]
            local body = {}
            i = i + 1
            while i <= #tokens do
                if tokens[i][1] == "endmacro" then break end
                table.insert(body, {tokens[i][1], tokens[i][2]})
                i = i + 1
            end
            if name then
                ctx.macros = ctx.macros or {}
                ctx.macros[name] = body
            end

        elseif cmd == "endmacro" or cmd == "erasemacro" then
            -- pass (handled by macro)

        -- Regular command: dispatch to kag table
        else
            -- Check if it's a macro invocation
            local macro_body = ctx.macros and ctx.macros[cmd]
            if macro_body then
                -- Expand macro inline — merge params
                local saved_tokens = tokens
                tokens = macro_body
                ctx.tokens = tokens
                i = 0
                tokens = saved_tokens  -- restore after macro body
            else
                -- text chunks become [ch] commands
                local handler = kag[cmd]
                local actual_cmd = cmd
                if not handler and type(cmd) == "string" and #cmd > 0 then
                    -- Unrecognized text → treat as [ch]
                    handler = kag["ch"]
                    if handler then
                        params = {text = cmd}
                        actual_cmd = "ch"
                    end
                end
                if handler then
                    local status, err = pcall(handler, ctx, params)
                    if not status then
                        -- Error → ErrorUI
                        local ErrorUI = require("Core.ErrorUI")
                        -- Lua-side error reporting
                        print("[ERROR] KAG command '" .. actual_cmd .. "' failed: " .. tostring(err))
                        if ctx.handle_error then
                            ctx.handle_error(actual_cmd, tostring(err), i)
                        end
                    end
                end
            end
        end

        ctx.token_index = i
        i = i + 1
        coroutine.yield()
    end
end

-- ── Resume from saved state ─────────────────────────────────────────────────

function scheduler.resume(ctx)
    if not ctx.tokens or not ctx.token_index then return end
    scheduler.run(ctx, ctx.tokens, ctx.token_index)
end

return scheduler

