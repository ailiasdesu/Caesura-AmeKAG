-- ===========================================================================
--  Caesura (AmeKAG) — tokenizer.lua
--  Spec [1.1]: LPeg-first KAG script parser with pure-Lua fallback.
--  Token types: "text" | "command" | "comment" | "macro_def" | "embed_lua"
--  Token shape per type:
--    text:      {type, line, content}
--    command:   {type, line, cmd, params}
--    comment:   {type, line, content}
--    macro_def: {type, line, macro_name, macro_body}
--    embed_lua: {type, line, lua_code}
-- ===========================================================================

local Tokenizer = {}

local lpeg_ok, lpeg = pcall(require, "lpeg")
if not lpeg_ok then lpeg = nil end

-- ===========================================================================
--  LPeg Grammar — Spec [1.1] compliant
--  Matches one token per line: comment | macro_def | embed_lua | command | text
-- ===========================================================================

local function buildLpegGrammar()
    local P, S, R, C, Ct, Cc = lpeg.P, lpeg.S, lpeg.R, lpeg.C, lpeg.Ct, lpeg.Cc
    local alpha    = R("az", "AZ") + P("_")
    local digit    = R("09")
    local alphanum = alpha + digit + P(".") + P("_")
    local space    = S(" \t")^0
    local eol      = P("\r\n") + P("\n") + P("\r") + -P(1)
    local not_eol  = 1 - eol

    -- Value: double-quoted | single-quoted | bare
    local dq_str   = P('"') * C((1 - P('"'))^0) * P('"')
    local sq_str   = P("'") * C((1 - P("'"))^0) * P("'")
    local bare_val = C((1 - S(" \t\r\n]"))^1)
    local value    = dq_str + sq_str + bare_val

    -- Attribute pairs: key=val, collected as flat table {k1,v1,k2,v2,...}
    local ident     = alpha * alphanum^0
    local attr_pair = C(ident) * P("=") * value
    local attr_list = Ct((attr_pair * (space * attr_pair)^0)^0)

    -- Command: [cmd_name key=val ...] or [/cmd_name]
    local cmd_name  = C((P("/")^-1 * alpha * alphanum^0))
    local command   = P("[") * space * cmd_name * space * attr_list * P("]")
    local cmd_line  = Ct(command * Cc("command"))

    -- Comment: ; to EOL
    local comment_line = Ct(C(P(";") * C(not_eol^0)) * Cc("comment"))

    -- Embedded Lua: @ to EOL
    local embed_line = Ct(C(P("@") * C(not_eol^0)) * Cc("embed_lua"))

    -- Macro definition: *name|body
    local macro_name = C((1 - P("|") - eol)^1)
    local macro_def_line = Ct(C(P("*") * macro_name * P("|") * C(not_eol^0)) * Cc("macro_def"))

    -- Text: anything else until ; [ @ * or EOL
    local text_content = C((1 - eol - S(";[@*"))^1)
    local text_line    = Ct(C(text_content) * Cc("text"))

    -- Line: optional whitespace, then one token, then optional EOL + whitespace
    local line = space * (comment_line + macro_def_line + embed_line + cmd_line + text_line) * eol^-1 * space
    local grammar = Ct(line^0)

    return grammar
end

local function lpegTokenize(text)
    if not lpeg then return nil end
    local ok, grammar = pcall(buildLpegGrammar)
    if not ok then return nil end
    local result = grammar:match(text)
    if not result or #result == 0 then
        -- LPeg match failed or empty; fall back to pure Lua
        return nil
    end
    return result
end

-- ===========================================================================
--  Pure-Lua fallback tokenizer
--  Recognizes: ;comment  @embed_lua  *macro|body  [command params]  text
-- ===========================================================================

local function pureLuaTokenize(text)
    local tokens = {}
    local line_no = 0
    local i = 1
    local len = #text

    while i <= len do
        local ch = text:sub(i, i)

        -- Skip horizontal whitespace
        while i <= len and (ch == " " or ch == "\t") do
            i = i + 1
            if i <= len then ch = text:sub(i, i) end
        end

        if i > len then break end

        -- Find end of line
        local j = i
        while j <= len and text:sub(j, j) ~= "\n" and text:sub(j, j) ~= "\r" do
            j = j + 1
        end

        local lineText = text:sub(i, j - 1)
        local next_i = j
        while next_i <= len and (text:sub(next_i, next_i) == "\n" or text:sub(next_i, next_i) == "\r") do
            next_i = next_i + 1
        end

        line_no = line_no + 1

        if #lineText == 0 then
            -- Empty line: skip
        elseif lineText:sub(1, 1) == ";" then
            -- Comment: skip (not emitted)
        elseif lineText:sub(1, 1) == "@" then
            table.insert(tokens, { type = "embed_lua", line = line_no, lua_code = lineText:sub(2) })
        elseif lineText:sub(1, 1) == "*" then
            local pipe = lineText:find("|", 2, true)
            if pipe then
                local mname = lineText:sub(2, pipe - 1):match("^%s*(.-)%s*$") or ""
                local mbody = lineText:sub(pipe + 1) or ""
                table.insert(tokens, { type = "macro_def", line = line_no, macro_name = mname, macro_body = mbody })
            end
        elseif lineText:sub(1, 1) == "[" then
            local closeBracket = lineText:find("]", 2, true)
            if closeBracket then
                local tagContent = lineText:sub(2, closeBracket - 1)
                local cmd, params = Tokenizer.parseTagParams(tagContent)
                table.insert(tokens, { type = "command", line = line_no, cmd = cmd, params = params })
            end
        else
            table.insert(tokens, { type = "text", line = line_no, content = lineText })
        end

        i = next_i
    end

    return tokens
end

-- ===========================================================================
--  Tokenizer.parseTagParams(tagContent) → cmd, params
--  Parses "bg storage=\"bg01\" blend=\"alpha\"" → "bg", {storage="bg01", blend="alpha"}
--  Handles quoted values, bare values, numeric coercion.
-- ===========================================================================

function Tokenizer.parseTagParams(tagContent)
    local len = #tagContent
    local pos = 1
    local parts = {}

    -- Split by whitespace, respecting quotes
    while pos <= len do
        while pos <= len and tagContent:sub(pos, pos):match("[ \t]") do pos = pos + 1 end
        if pos > len then break end

        local ch = tagContent:sub(pos, pos)
        if ch == '"' or ch == "'" then
            local quote = ch
            pos = pos + 1
            local startPos = pos
            while pos <= len and tagContent:sub(pos, pos) ~= quote do pos = pos + 1 end
            table.insert(parts, tagContent:sub(startPos, pos - 1))
            if pos <= len then pos = pos + 1 end -- skip closing quote
        else
            local startPos = pos
            while pos <= len and not tagContent:sub(pos, pos):match("[ \t]") do pos = pos + 1 end
            table.insert(parts, tagContent:sub(startPos, pos - 1))
        end
    end

    if #parts == 0 then return "", {} end

    local cmd = parts[1]
    local params = {}
    for i = 2, #parts do
        local token = parts[i]
        local eqPos = token:find("=", 1, true)
        if eqPos then
            local key = token:sub(1, eqPos - 1)
            local val = token:sub(eqPos + 1)
            local num = tonumber(val)
            params[key] = (num ~= nil) and num or val
        else
            -- Positional param (backward compat)
            params[i - 1] = token
        end
    end

    return cmd, params
end

-- ===========================================================================
--  Tokenizer.parse(script_text) → tokens, macros
--  Unified entry: tries LPeg first, falls back to pure Lua.
--  Normalizes both formats to the canonical token shape.
-- ===========================================================================

function Tokenizer.parse(script_text)
    local rawTokens = lpegTokenize(script_text) or pureLuaTokenize(script_text)

    local tokens = {}
    local macros = {}
    local lineNo = 0

    for _, entry in ipairs(rawTokens) do
        -- Detect format: pureLua has .type field; LPeg is Ct{...}
        if type(entry.type) == "string" then
            -- === Pure-Lua format ===
            local typ = entry.type
            if typ == "comment" then
                -- skip
            elseif typ == "macro_def" then
                macros[entry.macro_name] = entry.macro_body
                table.insert(tokens, {
                    type = "macro_def", line = entry.line,
                    macro_name = entry.macro_name, macro_body = entry.macro_body
                })
            elseif typ == "embed_lua" then
                table.insert(tokens, {
                    type = "embed_lua", line = entry.line,
                    lua_code = entry.lua_code
                })
            elseif typ == "command" then
                table.insert(tokens, {
                    type = "command", line = entry.line,
                    cmd = entry.cmd, params = entry.params or {}
                })
            elseif typ == "text" then
                if entry.content and #entry.content > 0 then
                    table.insert(tokens, {
                        type = "text", line = entry.line,
                        content = entry.content
                    })
                end
            end
        else
            -- === LPeg format ===
            -- Each entry is Ct{...captures..., type_tag}
            -- For command: {cmd_name_string, {k1,v1,k2,v2,...}, "command"}
            -- For macro_def: {matched, macro_name, body, "macro_def"}
            -- For embed_lua: {matched, lua_code, "embed_lua"}
            -- For text: {content_string, "text"}
            -- For comment: {matched, comment_text, "comment"}
            lineNo = lineNo + 1
            local typeTag = entry[#entry]

            if typeTag == "comment" then
                -- skip

            elseif typeTag == "macro_def" then
                local mname = (entry[2] or ""):match("^%s*(.-)%s*$") or ""
                local mbody = entry[3] or ""
                macros[mname] = mbody
                table.insert(tokens, {
                    type = "macro_def", line = lineNo,
                    macro_name = mname, macro_body = mbody
                })

            elseif typeTag == "embed_lua" then
                table.insert(tokens, {
                    type = "embed_lua", line = lineNo,
                    lua_code = entry[2] or ""
                })

            elseif typeTag == "command" then
                -- entry[1] = cmd_name (string), entry[2] = attr_list (flat table {k,v,k,v...})
                local cmdName = entry[1] or ""
                local params = {}

                if type(entry[2]) == "table" then
                    -- LPeg attr_list: flat table {key1, val1, key2, val2, ...}
                    for k = 1, #entry[2], 2 do
                        local key = entry[2][k]
                        local val = entry[2][k + 1]
                        if key then
                            local num = tonumber(val)
                            params[key] = (num ~= nil) and num or val
                        end
                    end
                end

                table.insert(tokens, {
                    type = "command", line = lineNo,
                    cmd = cmdName, params = params
                })

            elseif typeTag == "text" then
                local txt = entry[1] or ""
                if #txt > 0 then
                    table.insert(tokens, {
                        type = "text", line = lineNo,
                        content = txt
                    })
                end
            end
        end
    end

    return tokens, macros
end

return Tokenizer