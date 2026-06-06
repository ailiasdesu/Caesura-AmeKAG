-- =============================================================================
--  Caesura (AmeKAG) — tokenizer.lua (simplified grammar)
-- =============================================================================

local lpeg = require("lpeg")
local P, S, R, C, Ct, Cc = lpeg.P, lpeg.S, lpeg.R, lpeg.C, lpeg.Ct, lpeg.Cc

local tokenizer = {}

local space   = S(" \t\r\n")^0        -- optional whitespace
local ws1     = S(" \t\r\n")^1        -- required whitespace (for skip)
local dquote  = P('"')
local squote  = P("'")
local ident   = R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0

-- Quoted value with escape support for \" and \'
local dq_esc  = P("\\") * P('"')           -- \" escape
local sq_esc  = P("\\") * P("'")           -- \' escape
local qval    = dquote * C((dq_esc + (1 - dquote))^0) * dquote
              + squote * C((sq_esc + (1 - squote))^0) * squote
local uval    = C((1 - S(" \t\r\n]"))^1)
local param   = Ct(C(ident) * space * "=" * space * (qval + uval))

-- Command body: ["cmd", name, {{key,val},...}]
local cmd_body = Ct(Cc("cmd") * C(ident) * space * Ct(param * space)^0)
-- Text body: ["text", content]
local text_body = Ct(Cc("text") * C((1 - S("[") - P("\r") * P("\n"))^1))
local cmd_pat  = P("[") * space * cmd_body * space * P("]")

-- Skip: whitespace (1+) or comment (; ... optional newline), repeated
local comment = P(";") * (1 - S("\r\n"))^0 * (P("\r")^-1 * P("\n"))^-1
local skip    = (ws1 + comment)^0

-- BOM
local bom     = P("\239\187\191")^-1

-- Full grammar
local grammar = Ct(
    bom * skip *
    (cmd_pat + text_body) *
    (skip * (cmd_pat + text_body))^0 *
    skip * -1
)

function tokenizer.parse(ks_text)
    if not ks_text or ks_text == "" then return {} end
    -- Strip BOM
    if ks_text:sub(1,1) == "\239" then ks_text = ks_text:sub(4) end
    local raw = lpeg.match(grammar, ks_text)
    if not raw then
        error("Tokenizer: parse failed -- check .ks syntax")
    end
    local result = {}
    for _, t in ipairs(raw) do
        if type(t) == "table" and #t >= 2 then
            local typ = t[1]
            if typ == "cmd" then
                local params = {}
                -- t[3], t[4], ... are param tables from Ct(param*space)^0
                -- Each is {{key, value}} — unwrap one level
                for i = 3, #t do
                    if type(t[i]) == "table" then
                        local pair = t[i][1]  -- unwrap Ct(param*space) layer
                        if type(pair) == "table" and pair[1] then
                            params[#params + 1] = pair
                        end
                    end
                end
                result[#result + 1] = { type = "command", cmd = t[2], params = params }
            elseif typ == "text" then
                local txt = t[2] or ""
                if not txt:match("^%s*$") then
                    result[#result + 1] = { type = "text", content = txt }
                end
            end
        end
    end
    return result
end

function tokenizer.parse_file(path)
    local f = io.open(path, "r")
    if not f then error("Tokenizer: cannot open file: " .. path) end
    local content = f:read("*a")
    f:close()
    return tokenizer.parse(content)
end

return tokenizer
