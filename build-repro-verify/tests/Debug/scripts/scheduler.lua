-- =============================================================================
--  Caesura (AmeKAG) ?? scheduler.lua
--  Token stream executor. Iterates tokens, dispatches to kag[cmd](ctx, params),
--  handles flow-control inline (if/jump/call/return/label/end/macro/eval/wait).
--  Coroutine-based: yields on blocking ops, resumes next frame from token_index.
-- =============================================================================

-- [R4-FIX] Note: The [button]/[endbutton] choice/branch system is implemented
-- purely in Lua (kag/commands/text.lua). Flow control here handles [switch]/[case]
-- for data-driven branching. See text.lua for interactive user choices.

local scheduler = {}

-- ???? Flow-control command set (handled inline, never dispatched to kag table) ????

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

-- ???? Internal helpers ????????????????????????????????????????????????????????????????????????????????????????????????????????????????

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

-- ???? Main execution loop ??????????????????????????????????????????????????????????????????????????????????????????????????????????

function scheduler.run(ctx, tokens, start_index)
    if not tokens or #tokens == 0 then return end

    -- Normalize token format: tokenizer returns {type, cmd, params}
    -- scheduler expects {[1]=cmd, [2]=params}
    -- If first token has .type field, convert all to array format
    if tokens[1] and tokens[1].type then
        for j, t in ipairs(tokens) do
            if t.type then
                if t.type == "label" then
                    tokens[j] = { "label", { name = t.name } }
            elseif t.type == "iscript" then
                tokens[j] = { "iscript", { body = t.body or "" } }
            else
                tokens[j] = { t.cmd or t.type, t.params or {} }
                end
            end
        end
    end
    -- Normalize params: convert array-format {{key,val},...} to named access
    -- so commands can use params.name, params.target, etc.
    for j, t in ipairs(tokens) do
        local params = t[2]
        if type(params) == "table" then
            for _, p in ipairs(params) do
                if type(p) == "table" and type(p[1]) == "string" then
                    params[p[1]] = p[2]
                end
            end
        end
    end

    local kag = require("kag")
    local operation = require("kag.operation")
    start_index = start_index or 1

    local i = start_index
    while i <= #tokens do
        local tok = tokens[i]
        local cmd = tok[1]
        local params = tok[2] or {}

        -- Check stop flag
        if ctx.stop_flag then return end

        -- Check for Lua-initiated flow control (from [iscript] or external Lua)
        if ctx._next_index then
            i = ctx._next_index
            ctx._next_index = nil
        end

        -- Flow control: [jump]
        if cmd == "jump" then
            local target = params.target or params.label or params.storage
            if not target then
                print("[WARN] [jump] missing target/label/storage parameter")
            elseif params.target and target:sub(1,1) ~= "*" then
                -- Cross-scene jump: load new scene file
                local path = "assets/script/" .. target
                local new_tokens = ctx.load_tokens and ctx.load_tokens(path)
                if new_tokens then
                    ctx.tokens = new_tokens
                    ctx.token_index = 1
                    ctx.current_scene = path
                    ctx.call_stack = {}
                    ctx.layers = {}
                    ctx.backlog = {}
                    operation.cancel_all(ctx)
                    return
                else
                    print("[WARN] [jump] failed to load scene: " .. path)
                end
            else
                -- Intra-scene jump: find label (target may be "label" or "*label")
                local label = target:gsub("^*", "")  -- strip leading * if present
                local idx = find_label(tokens, label)
                if idx then
                    i = idx
                else
                    print("[WARN] [jump] label not found: " .. label)
                end
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
            operation.cancel_all(ctx)
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

        -- Flow control: [label] ?? no-op, used by jump/call
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
            -- switch/case dispatch (Alpha: flat only, single-level)
            local switchExpr = params[1] or ""
            local switchVal = nil
            if ctx.variables and ctx.variables[switchExpr] ~= nil then
                switchVal = tostring(ctx.variables[switchExpr])
            else
                switchVal = switchExpr
            end

            -- Scan forward to find matching case
            local caseStart = nil
            local scanIdx = i
            while scanIdx <= #tokens do
                local stok = tokens[scanIdx]
                if stok[1] == "case" then
                    local caseParams = stok[2] or {}
                    local caseVal = caseParams[1] or ""
                    if tostring(caseVal) == switchVal then
                        caseStart = scanIdx + 1
                        break
                    end
                elseif stok[1] == "default" then
                    caseStart = scanIdx + 1
                    break
                elseif stok[1] == "endswitch" then
                    break
                end
                scanIdx = scanIdx + 1
            end

            if caseStart then
                i = caseStart  -- jump into case body, scheduler.run will execute
            else
                -- No match: skip to endswitch
                while i <= #tokens and tokens[i][1] ~= "endswitch" do
                    i = i + 1
                end
            end

        elseif cmd == "case" or cmd == "default" or cmd == "endswitch" then
            -- pass (handled by [switch] above)

        -- Flow control: [iscript] — inline Lua code block
        elseif cmd == "iscript" then
            local code = params.body or ""
            if #code > 0 then
                local sandbox = {
                    ctx       = ctx,
                    f         = ctx.f or {},
                    sf        = ctx.sf or {},
                    tf        = ctx.tf or {},
                    kag       = require("kag"),
                    math      = math,
                    string    = string,
                    table     = table,
                    os        = { clock = os.clock, date = os.date, time = os.time },
                    tostring  = tostring,
                    tonumber  = tonumber,
                    type      = type,
                    pairs     = pairs,
                    ipairs    = ipairs,
                    next      = next,
                    print     = print,
                    pcall     = pcall,
                    select    = select,
                    unpack    = unpack or table.unpack,
                    error     = error,
                    coroutine  = coroutine,
                }
                local fn, compileErr = load(code, "=iscript", "t", sandbox)
                if fn then
                    local ok, runtimeErr = pcall(fn)
                    if not ok then
                        print("[iscript] Runtime error: " .. tostring(runtimeErr))
                    end
                else
                    print("[iscript] Compile error: " .. tostring(compileErr))
                end
            end

        -- Flow control: [eval] — unified scope (ctx + f + sf + tf)
        elseif cmd == "eval" then
            local code = params.exp or params.code or ""
            local env = {
                ctx = ctx,
                f   = ctx.f or {},
                sf  = ctx.sf or {},
                tf  = ctx.tf or {},
            }
            setmetatable(env, { __index = _G })
            local fn, compileErr = load(code, "=eval", "t", env)
            if fn then
                local ok, result = pcall(fn)
                if ok and result ~= nil then
                    ctx.tf = ctx.tf or {}
                    ctx.tf.eval_result = result
                end
            else
                print("[eval] Compile error: " .. tostring(compileErr))
            end

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

        elseif cmd == "erasemacro" then
            local name = params.name
            if name and ctx.macros then
                ctx.macros[name] = nil
            end
        elseif cmd == "endmacro" then
            -- pass (handled by macro recording above)

        -- Regular command: dispatch to kag table
        else
            -- Check if it's a macro invocation
            local macro_body = ctx.macros and ctx.macros[cmd]
            if macro_body then
                -- Splice macro body into token stream, replacing the invocation
                local new_tokens = {}
                for n = 1, i - 1 do
                    table.insert(new_tokens, tokens[n])
                end
                for _, bt in ipairs(macro_body) do
                    table.insert(new_tokens, {bt[1], bt[2]})
                end
                for n = i + 1, #tokens do
                    table.insert(new_tokens, tokens[n])
                end
                tokens = new_tokens
                ctx.tokens = tokens
                i = i - 1  -- will point to first body token after i = i + 1
            else
                -- text chunks become [ch] commands
                local handler = kag[cmd]
                local actual_cmd = cmd
                if not handler and type(cmd) == "string" and #cmd > 0 then
                    -- Unrecognized text ?? treat as [ch]
                    handler = kag["ch"]
                    if handler then
                        params = {text = cmd}
                        actual_cmd = "ch"
                    end
                end
                if handler then
                    local status, err = pcall(handler, ctx, params)
                    if not status then
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

-- ???? Resume from saved state ??????????????????????????????????????????????????????????????????????????????????????????????????

-- [R7-FIX] Read Skip: if skip_mode is "seen", only advance past already-seen tokens
-- Usage: [skip mode=seen] to skip only previously-read text
-- NOTE: Full implementation requires hooking into the token advance loop.
-- Current implementation: context variable is set so Lua scripts can check it.

function scheduler.resume(ctx)
    if not ctx.tokens or not ctx.token_index then return end
    scheduler.run(ctx, ctx.tokens, ctx.token_index)
end

return scheduler
