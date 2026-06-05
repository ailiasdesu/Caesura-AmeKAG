-- ===========================================================================
--  Caesura (AmeKAG) — kag/parser.lua
--  KAG script parser — legacy backward-compatibility wrapper.
--  [P1] NOTE: This module is superseded by tokenizer.lua (spec [1.1]).
--  New code should use tokenizer.lua directly. This wrapper maps the old
--  parse() output format for existing test/legacy callers.
-- ===========================================================================

local Parser = {}
local Tokenizer = require("tokenizer")

-- Legacy token types for backward compat
Parser.TOKEN = {
    TAG     = 1,
    TEXT    = 2,
    COMMENT = 3,
    EOF     = 4,
}

--- Parser.parse(text) → { type="tag"|"text", command?, params?, text? }[]
--- Maps tokenizer.lua output to legacy conductor.lua format.
function Parser.parse(text)
    local tokens, _ = Tokenizer.parse(text)
    local commands = {}

    for _, tok in ipairs(tokens) do
        if tok.type == "text" then
            table.insert(commands, {
                type = "text",
                text = tok.content or "",
            })
        elseif tok.type == "command" then
            table.insert(commands, {
                type = "tag",
                command = tok.cmd,
                params = tok.params or {},
            })
        elseif tok.type == "embed_lua" then
            -- Treat embedded Lua as a tag for conductor compat
            table.insert(commands, {
                type = "tag",
                command = "eval",
                params = { exp = tok.lua_code or "" },
            })
        end
        -- Comments and macro_defs are skipped (legacy behavior)
    end

    return commands
end

return Parser