-- =============================================================================
--  Caesura (AmeKAG) — tokenizer.lua (KAG 3.0 full grammar)
-- =============================================================================
--  Design: cmd_pat (generic [command param=value ...]) handles 95% of KAG 3.0
--  tags. Only non-standard syntax (labels, inline code, block markers) needs
--  explicit LPeg patterns. This keeps the grammar compact while covering all
--  167 KAG 3.0 commands.
-- =============================================================================

local lpeg = require("lpeg")
local P, S, R, C, Ct, Cc, Cg, Cp = lpeg.P, lpeg.S, lpeg.R, lpeg.C, lpeg.Ct, lpeg.Cc, lpeg.Cg, lpeg.Cp

local tokenizer = { _VERSION = "2.0.0" }

local space   = S(" \t\r\n")^0        -- optional whitespace
local ws1     = S(" \t\r\n")^1        -- required whitespace
local dquote  = P('"')
local squote  = P("'")
local ident   = R("az", "AZ", "__") * R("az", "AZ", "09", "__")^0

-- Quoted value with escape support for \" and \'
local dq_esc  = P("\\") * P('"')           -- \" escape
local sq_esc  = P("\\") * P("'")           -- \' escape
local qval    = dquote * C((dq_esc + (1 - dquote))^0) * dquote
              + squote * C((sq_esc + (1 - squote))^0) * squote
local uval    = C((1 - S(" \t\r\n]"))^1)
-- Bare positional value (KAG3 syntax: [delay 500], [se 1], [gallery 2]):
-- params[1] gets the raw string; handlers tonumber it. Comes AFTER the
-- ident=value branch so "x=5" never parses as a bare value.
local param   = Ct(C(ident) * space * "=" * space * (qval + uval))
              + Ct(Cc("1") * C(uval))

-- Command body: ["cmd", name, {{key,val},...}]
local cmd_body = Ct(Cc("cmd") * C(ident) * space * Ct(param * space)^0)
-- Text body: ["text", content]
local text_body = Ct(Cc("text") * C((1 - S("[\r\n*"))^1))
local cmd_pat  = P("[") * space * cmd_body * space * P("]")

-- Skip: whitespace (1+) or comment (; ... optional newline), repeated
local comment = P(";") * (1 - S("\r\n"))^0 * (P("\r")^-1 * P("\n"))^-1
local skip    = (ws1 + comment)^0

-- BOM
local bom     = P("\239\187\191")^-1

-- Label: *identifier (star-prefixed label marker)
local label_pat = Ct(Cc("label") * P("*") * C(ident))

-- Explicit command patterns for non-standard syntax
local wsp   = space
local attrs = Ct(param * space)^0

-- Audio/flow commands with special handling
-- Command-prefix boundary: after "[se" the next char must not be an
-- identifier char, else P_se would match "[setbgmvolume ...]" once the
-- bare-value param branch swallowed the tail (C++ demo e2e regression).
-- NOTE: the pure-Lua lpeg's unary minus is OPTIONAL, not a negative
-- lookahead -- a zero-width function pattern does the real boundary.
local function cmd_pre(s)
    return P("[" .. s) * P(function(sub, pos)
        -- review should-fix: "[%s%]=]" is a CLASS{ws,=} + literal ']'
        -- (Lua 5.4 classend) -- a 2-char pattern that can never match a
        -- 1-char c, making the boundary dead code. %s alone is safe.
        local c = sub:sub(pos, pos)
        if c == "" or c == " " or c == "\t" or c == "\r" or c == "\n"
           or c == "=" or c == "]" then
            return { pos }
        end
        return nil
    end)
end

local P_se        = Ct(cmd_pre("se")      * wsp * Cc("cmd") * Cc("se")        * attrs * P"]")
local P_stopse    = Ct(cmd_pre("stopse")  * P"]"   * Cc("cmd") * Cc("stopse")   * Cc({}))
local P_fadebgm   = Ct(cmd_pre("fadebgm") * wsp * Cc("cmd") * Cc("fadebgm")   * attrs * P"]")
local P_fadevoice = Ct(cmd_pre("fadevoice") * wsp * Cc("cmd") * Cc("fadevoice") * attrs * P"]")
local P_fadese    = Ct(cmd_pre("fadese")  * wsp * Cc("cmd") * Cc("fadese")    * attrs * P"]")
local P_wait      = Ct(cmd_pre("wait")    * wsp * Cc("cmd") * Cc("wait")      * attrs * P"]")
local P_delay     = Ct(cmd_pre("delay")   * wsp * Cc("cmd") * Cc("delay")     * attrs * P"]")
local P_skip      = Ct(cmd_pre("skip")    * wsp * Cc("cmd") * Cc("skip")      * attrs * P"]")

-- Inline expression
local eval_pat = Ct(cmd_pre("eval") * wsp * Cc("cmd") * Cc("eval") * attrs * P"]")

-- Embedded script block: [iscript] ... [endscript]
-- Captured as single token with raw body for Lua execution.
local iscript_close = P"[endscript]" + P"[/endscript]"
local iscript_body = C((P(1) - P"[endscript]" - P"[/endscript]")^0)
local iscript_pat = Ct(Cc("iscript") * P"[iscript]" * skip * iscript_body * skip * iscript_close)

-- Full grammar: explicit patterns first, then generic cmd_pat, then label/text
local explicit_cmds = iscript_pat + eval_pat + P_se + P_stopse + P_fadebgm + P_fadevoice + P_fadese + P_wait + P_delay + P_skip + cmd_pat

local grammar = Ct(
    bom * skip *
    (explicit_cmds + label_pat + text_body) *
    (skip * (explicit_cmds + label_pat + text_body))^0 *
    skip * -1
)

-- One-token pattern with an END-position capture: used by parse_with_offsets
-- to advance through the source token by token (LPeg's init argument gives
-- the next match start; the captured end position is the offset).
-- Two Cp() captures: the first records the position AFTER leading
-- whitespace/comments (the command's own start -- the exact byte of
-- '['), the second the match end for advancing. Audit: the old form
-- reported the match START including leading comments, so editor
-- jumps landed on the comment, not the command.
local one_token = Ct(skip * Cp() * (explicit_cmds + label_pat + text_body) * Cp())

-- ---- Normalization ----
local function normalize(raw_tokens)
    local result = {}
    for _, t in ipairs(raw_tokens) do
        if type(t) ~= "table" or #t < 2 then goto continue end
        local typ = t[1]
        if typ == "cmd" then
            local params = {}
            for i = 3, #t do
                if type(t[i]) == "table" then
                    local pair = t[i][1]
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
        elseif typ == "label" then
            result[#result + 1] = { type = "label", name = t[2] }
        elseif typ == "iscript" then
            result[#result + 1] = { type = "iscript", body = t[2] or "" }
        end
        ::continue::
    end
    return result
end

--- tokenizer.parse_with_offsets(text) → tokens with a byte `offset` each.
--  Advances through the source with lpeg.match(one_token, text, init): the
--  LPeg `init` argument starts each match after the previous token's end,
--  and a Cp() end-position capture yields the exact byte offset. Comments
--  and blank lines are consumed by the pattern between tokens, so offsets
--  are source-accurate (the durable fix for ks_check line numbers).
function tokenizer.parse_with_offsets(ks_text)
    if not ks_text or ks_text == "" then return {} end
    if ks_text:byte(1) == 0xEF and ks_text:byte(2) == 0xBB and ks_text:byte(3) == 0xBF then
        ks_text = ks_text:sub(4)
    end
    local out = {}
    local init = 1
    while true do
        local cap = lpeg.match(one_token, ks_text, init)
        if not cap then break end
        local startpos = cap[1] or init  -- after leading skip (cmd's '[')
        local t = cap[2]                -- the token capture table
        local endpos = cap[3] or init   -- match end
        if type(t) == "table" and t[1] then
            local tok = { offset = startpos, end_offset = endpos - 1 }
            local typ = t[1]
            if typ == "cmd" then
                tok.type = "command"; tok.cmd = t[2]; tok.params = t[3] or {}
            elseif typ == "text" then
                tok.type = "text"; tok.content = t[2] or ""
            elseif typ == "label" then
                tok.type = "label"; tok.name = t[2]
            elseif typ == "iscript" then
                tok.type = "iscript"; tok.body = t[2] or ""
            end
            out[#out + 1] = tok
        end
        -- Cp() returns the position AFTER the match (index of the next
        -- char) -- advancing +1 skips a byte and a mid-line command
        -- ([ch] following text on the same line) would be re-tokenized
        -- as text (review blocking). endpos IS the next start.
        init = endpos
        if init > #ks_text then break end
    end
    return out
end

function tokenizer.parse(ks_text)
    if not ks_text or ks_text == "" then return {} end
    -- Strip BOM
    -- BOM is 3 bytes (EF BB BF). Grammar also has optional BOM prefix.
    if ks_text:byte(1)==0xEF and ks_text:byte(2)==0xBB and ks_text:byte(3)==0xBF then
        ks_text = ks_text:sub(4) end
    -- Locate the failure offset for actionable errors: try the grammar,
    -- then report where the longest successful prefix stopped.
    local raw = lpeg.match(grammar, ks_text)
    if not raw then
        -- Report the failing line via binary search on the longest
        -- successful prefix (grammar matches prefix-free up to the error).
        local lo, hi = 1, #ks_text
        while lo < hi do
            local mid = math.floor((lo + hi + 1) / 2)
            if lpeg.match(grammar, ks_text:sub(1, mid)) then
                lo = mid
            else
                hi = mid - 1
            end
        end
        local line = 1
        for i = 1, lo do
            if ks_text:byte(i) == 10 then line = line + 1 end
        end
        error(string.format("Tokenizer: parse failed near line %d (byte %d)",
                            line, lo))
    end
    return normalize(raw)
end

function tokenizer.parse_file(path)
    local f = io.open(path, "r")
    if not f then error("Tokenizer: cannot open file: " .. path) end
    -- Cap scene size (16MB) so a corrupted/giant .ks cannot exhaust the
    -- Lua memory limit and trigger the fatal-error dialog.
    f:seek("end")
    local size = f:seek("cur")
    f:seek("set")
    if size > 16 * 1024 * 1024 then
        f:close()
        error("Tokenizer: scene too large (>" .. (16 * 1024 * 1024) ..
              " bytes): " .. path)
    end
    local content = f:read("*a")
    f:close()
    return tokenizer.parse(content)
end

return tokenizer
