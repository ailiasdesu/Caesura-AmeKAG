-- ===========================================================================
--  Caesura (AmeKAG) — flow.lua
--  Spec [1.3][1.4]: Jump/call/return/link/end + conditional branches.
--  Pure Lua logic — no backend binding dependency.
-- ===========================================================================

local Flow = {}

-- ===========================================================================
--  Variable lookup: f.xxx (global flag), sf.xxx (system flag), tf.xxx (temp)
-- ===========================================================================

local function lookupVar(exp, ctx)
    -- f.xxx → global flags
    local fkey = exp:match("^f%.(.+)$")
    if fkey then
        ctx.f = ctx.f or {}
        return ctx.f[fkey]
    end

    -- sf.xxx → system flags
    local sfkey = exp:match("^sf%.(.+)$")
    if sfkey then
        ctx.sf = ctx.sf or {}
        return ctx.sf[sfkey]
    end

    -- tf.xxx → temp flags
    local tfkey = exp:match("^tf%.(.+)$")
    if tfkey then
        ctx.tf = ctx.tf or {}
        return ctx.tf[tfkey]
    end

    -- Direct ctx field lookup
    local val = ctx[exp]
    if val ~= nil then return val end

    return nil
end

-- ===========================================================================
--  Evaluate expression string supporting:
--    f.xxx, sf.xxx, tf.xxx variable prefixes
--    Operators: ==, !=, >=, <=, >, <, &&, ||
--    Booleans: true, false
--    Numeric and string comparisons
-- ===========================================================================

function Flow.evalExpr(exp, ctx)
    if not exp or exp == "" then return true end

    -- Trim
    exp = exp:match("^%s*(.-)%s*$") or exp

    -- Boolean literals
    if exp == "true" then return true end
    if exp == "false" then return false end
    if exp == "nil" then return false end

    -- Handle || (OR) — short-circuit
    local orParts = {}
    local depth = 0
    local last = 1
    for i = 1, #exp - 1 do
        local c1, c2 = exp:sub(i, i), exp:sub(i + 1, i + 1)
        if c1 == "(" then depth = depth + 1
        elseif c1 == ")" then depth = depth - 1
        elseif c1 == "|" and c2 == "|" and depth == 0 then
            table.insert(orParts, exp:sub(last, i - 1))
            last = i + 2
        end
    end
    table.insert(orParts, exp:sub(last))
    if #orParts > 1 then
        for _, part in ipairs(orParts) do
            if Flow.evalExpr(part, ctx) then return true end
        end
        return false
    end

    -- Handle && (AND) — short-circuit
    local andParts = {}
    depth = 0
    last = 1
    for i = 1, #exp - 1 do
        local c1, c2 = exp:sub(i, i), exp:sub(i + 1, i + 1)
        if c1 == "(" then depth = depth + 1
        elseif c1 == ")" then depth = depth - 1
        elseif c1 == "&" and c2 == "&" and depth == 0 then
            table.insert(andParts, exp:sub(last, i - 1))
            last = i + 2
        end
    end
    table.insert(andParts, exp:sub(last))
    if #andParts > 1 then
        for _, part in ipairs(andParts) do
            if not Flow.evalExpr(part, ctx) then return false end
        end
        return true
    end

    -- Strip parentheses
    if exp:sub(1, 1) == "(" and exp:sub(#exp, #exp) == ")" then
        return Flow.evalExpr(exp:sub(2, #exp - 1), ctx)
    end

    -- Try operators: ==, !=, ~=, >=, <=, >, <
    local patterns = {
        {"!=", "!="}, {"~=", "~="}, {"==", "=="}, {">=", ">="}, {"<=", "<="}, {">", ">"}, {"<", "<"}
    }

    for _, pat in ipairs(patterns) do
        local op = pat[1]
        local pos = nil
        -- Find operator not inside quotes
        local inStr = false
        local quoteCh = nil
        for i = 1, #exp - #op + 1 do
            local ch = exp:sub(i, i)
            if (ch == '"' or ch == "'") and not inStr then
                inStr = true
                quoteCh = ch
            elseif inStr and ch == quoteCh then
                inStr = false
                quoteCh = nil
            elseif not inStr and exp:sub(i, i + #op - 1) == op then
                pos = i
                break
            end
        end

        if pos then
            local left = exp:sub(1, pos - 1):match("^%s*(.-)%s*$") or ""
            local right = exp:sub(pos + #op):match("^%s*(.-)%s*$") or ""

            -- Resolve values
            local lVal = tonumber(left) or left:match('^"(.-)"$') or left:match("^'(.-)'$") or lookupVar(left, ctx)
            local rVal = tonumber(right) or right:match('^"(.-)"$') or right:match("^'(.-)'$") or lookupVar(right, ctx)

            if lVal == nil or rVal == nil then return false end

            if op == "==" then return lVal == rVal
            elseif op == "!=" or op == "~=" then return lVal ~= rVal
            elseif op == ">=" then return tonumber(lVal) >= tonumber(rVal)
            elseif op == "<=" then return tonumber(lVal) <= tonumber(rVal)
            elseif op == ">" then return tonumber(lVal) > tonumber(rVal)
            elseif op == "<" then return tonumber(lVal) < tonumber(rVal)
            end
        end
    end

    -- Single value: truthy check
    local val = tonumber(exp) or lookupVar(exp, ctx)
    if val ~= nil then
        if type(val) == "boolean" then return val end
        if type(val) == "number" then return val ~= 0 end
        if type(val) == "string" then return #val > 0 end
        return val ~= nil
    end

    return false
end

-- ===========================================================================
--  Conditional if/elif/else/endif (Spec [1.4])
--  Returns nil to continue, or {skipType, ...} for flow control
-- ===========================================================================

function Flow.if_(ctx, params)
    local result = Flow.evalExpr(params.exp, ctx)
    if result then
        ctx.ifSkip = false
    else
        ctx.ifSkip = true
    end
end

function Flow.elif_(ctx, params)
    if ctx.ifSkip then
        local result = Flow.evalExpr(params.exp, ctx)
        if result then ctx.ifSkip = false end
    else
        ctx.ifSkip = true  -- already in true branch; skip rest
    end
end

-- [elseif] is the krkrz KAG tag name; alias for [elif]
Flow.elseif_ = Flow.elif_

Flow.else_ = function(ctx, params)
    ctx.ifSkip = not ctx.ifSkip  -- toggle
end

Flow.endif = function(ctx, params)
    ctx.ifSkip = false
end

-- ===========================================================================
--  Switch / case / default / endswitch (Spec [1.4])
-- ===========================================================================

function Flow.switch_(ctx, params)
    local val = params.exp or params.value or params[1]
    ctx.switchValue = val
    ctx.switchMatched = false
    ctx.switchActive = true
end

function Flow.case_(ctx, params)
    if not ctx.switchActive then return end
    local caseVal = params.exp or params.value or params[1]
    if ctx.switchMatched then
        ctx.ifSkip = true
        return
    end
    if tostring(caseVal) == tostring(ctx.switchValue) then
        ctx.switchMatched = true
        ctx.ifSkip = false
    else
        ctx.ifSkip = true
    end
end

function Flow.default_(ctx, params)
    if not ctx.switchActive then return end
    if ctx.switchMatched then
        ctx.ifSkip = true
    else
        ctx.switchMatched = true
        ctx.ifSkip = false
    end
end

function Flow.endswitch_(ctx, params)
    ctx.switchActive = false
    ctx.switchMatched = false
    ctx.switchValue = nil
    ctx.ifSkip = false
end

-- ===========================================================================
--  Jump / Call / Return / Link / Goto (Spec [1.3])
--  Returns signals: "jump" with {resume_at, tokens?, labelMap?}
--                   "stop" for top-level end
-- ===========================================================================

function Flow.jump(ctx, params)
    local target = params.target or params.label or params.storage or params[1]
    if not target then return nil end
    local idx = ctx.labelMap and ctx.labelMap[target]
    if idx then
        -- +1 because caller increments i after handling
        return "jump", { resume_at = idx + 1 }
    end
    print("[Flow] Jump target not found: " .. (target or "nil"))
    return nil
end

function Flow.call(ctx, params)
    local target = params.target or params.label or params.storage or params[1]
    if not target then return nil end
    local idx = ctx.labelMap and ctx.labelMap[target]
    if idx then
        -- Push return address (next instruction index)
        ctx.callStack = ctx.callStack or {}
        table.insert(ctx.callStack, ctx.token_ptr + 1)
        return "jump", { resume_at = idx + 1 }
    end
    print("[Flow] Call target not found: " .. (target or "nil"))
    return nil
end

function Flow.return_(ctx, params)
    ctx.callStack = ctx.callStack or {}
    if #ctx.callStack > 0 then
        local retAddr = table.remove(ctx.callStack)
        return "jump", { resume_at = retAddr }
    end
    -- Top-level return → stop
    return "stop"
end

function Flow.link(ctx, params)
    -- Jump to another .ks file
    -- Returns "jump" with new tokens parsed from target file
    local target = params.target or params.storage or params[1]
    if not target then return nil end

    local tokenizer = require("tokenizer")
    -- Try to read the target file (file name resolution handled by caller)
    local filePath = target
    if not target:match("%.ks$") then filePath = target .. ".ks" end

    -- Read file via ctx callback or backend
    local content = nil
    if ctx.readFile then
        content = ctx.readFile(filePath)
    end
    if not content then
        print("[Flow] Link target not found: " .. filePath)
        return nil
    end

    local newTokens, newMacros = tokenizer.parse(content)
    ctx.macros = newMacros

    -- Build label map for new tokens
    local newLabels = {}
    for i, tok in ipairs(newTokens) do
        if tok.type == "command" and tok.cmd == "label" and tok.params and tok.params.name then
            newLabels[tok.params.name] = i
        end
    end

    return "jump", {
        resume_at = 1,
        tokens = newTokens,
        labelMap = newLabels
    }
end

function Flow.goto_(ctx, params)
    -- [goto] is an alias for [jump] in krkrz KAG
    return Flow.jump(ctx, params)
end

function Flow.end_(ctx, params)
    return "stop"
end

return Flow