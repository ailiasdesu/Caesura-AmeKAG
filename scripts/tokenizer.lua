-- =============================================================================
--  Caesura (AmeKAG) — tokenizer.lua
--  LPeg-based .ks parser: text → token stream.
--  KAG syntax: [command param="value"] or @command param="value"
--  Text outside brackets is emitted as [ch] tokens.
-- =============================================================================

local lpeg = require("lpeg")
local P, S, V, R, C, Ct, Cc = lpeg.P, lpeg.S, lpeg.V, lpeg.R, lpeg.C, lpeg.Ct, lpeg.Cc

local tokenizer = {}

-- ── Grammar ────────────────────────────────────────────────────────────────

local space      = S(" \t\r\n")^0
local eq         = space * "=" * space
local dquote     = P('"')
local squote     = P("'")
local ident      = R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0

-- Quoted value: "..." or '...'
local qvalue     = dquote * C((1 - dquote)^0) * dquote
                 + squote * C((1 - squote)^0) * squote

-- Unquoted value: non-whitespace, not ]
local uvalue     = C((1 - S(" \t\r\n]"))^1)

-- Parameter: key = "value" | key = 'value' | key = value
local param      = Ct(ident * eq * (qvalue + uvalue) * space)

-- Command tag: [command param=val ...]
local cmd_tag    = P("[") * space * ident * space * Ct(param^0) * space * P("]")
local cmd        = Ct(C(ident) * C(param))

-- At-command: @command param=val (krkrz compat)
local at_cmd_tag = P("@") * space * ident * space * Ct(param^0) * space * (P("\r")^-1 * P("\n") + -1)
local at_cmd     = Ct(C(ident) * C(param))

-- Text outside brackets (up to next [ or @ or end)
local text_chunk = C((1 - S("[@") - P("\r") * P("\n"))^1)

-- Comment: ;comment  (skip)
local comment    = P(";") * (1 - S("\r\n"))^0

-- Newline
local newline    = P("\r")^-1 * P("\n")

-- Whitespace or comment
local skip       = (space + comment * newline^-1)^0

-- Token: command, text, or newline
local token      = Ct(cmd_tag * cmd)
                 + Ct(at_cmd_tag * at_cmd)
                 + Ct(text_chunk)
                 + Ct(newline * Cc("br") * Ct({}))

-- Full grammar
local grammar    = Ct(skip * token^0 * skip * -1)

-- ── Public API ─────────────────────────────────────────────────────────────

function tokenizer.parse(ks_text)
    if not ks_text or ks_text == "" then return {} end
    local result = lpeg.match(grammar, ks_text)
    if not result then
        error("Tokenizer: parse failed — check .ks syntax")
    end
    return result
end

function tokenizer.parse_file(path)
    local f = io.open(path, "r")
    if not f then
        error("Tokenizer: cannot open file: " .. path)
    end
    local content = f:read("*a")
    f:close()
    return tokenizer.parse(content)
end

return tokenizer
