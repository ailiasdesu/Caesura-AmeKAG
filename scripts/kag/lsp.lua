-- =============================================================================
--  Caesura (AmeKAG) — kag/lsp.lua
--  KAG Neo-Genesis language service (Battle 2): completion / hover /
--  diagnostics for the IDE. Pure Lua, driven by the declarative command
--  contracts (schema.dumpContracts is the single source of truth).
--
--  The IDE calls this through the engine's /api/eval endpoint (a Lua
--  expression returning a JSON string); the renderer parses it once.
--  Three entry points:
--    lsp.completion(line_text)      -> [{label, kind, detail, insertText}]
--    lsp.hover(cmd, param?)         -> {title, text}  (contract + desc)
--    lsp.diagnostics(text)          -> [{line, col, message, severity}]
--
--  All functions are pure (no ctx, no I/O) except diagnostics which
--  tokenizes the given source text.
-- =============================================================================

local lsp = {}

local schemaModule = require("kag.schema")
local tokenizer = require("tokenizer")

-- Load every command module so their contracts register (same pattern
-- as ks_check.lua; the contract registry is the completion/hover
-- source). No-op when already loaded.
pcall(require, "kag.commands.text")
pcall(require, "kag.commands.system")
pcall(require, "kag.commands.audio")
pcall(require, "kag.commands.transition")
pcall(require, "kag.commands.layer")
pcall(require, "kag.commands.vfx")
pcall(require, "kag.commands.video")
pcall(require, "kag.commands.save")

-- Completion item kinds (Monaco LanguageIdentifier kinds; numeric values
-- match monaco.languages.CompletionItemKind).
local KIND = {
    Function = 3,  -- command tag
    Field = 5,     -- parameter
    Keyword = 14,  -- flow control
}

-- Flow-control commands handled by the scheduler (completions too).
local FLOW_CMDS = {
    "if", "elseif", "else", "endif", "while", "endwhile", "for", "endfor",
    "break", "continue", "jump", "call", "return", "link", "label",
    "macro", "endmacro", "erasemacro", "switch", "case", "endcase",
    "default", "endswitch", "eval", "emb", "iscript", "endscript",
    "select", "sel", "endselect", "end", "stop",
}

--- Build the command index once (contracts + flow commands + kag table).
local commands = nil
local function ensure_index()
    if commands then return commands end
    commands = {}
    for cmd in pairs(schemaModule.dumpContracts()) do
        commands[cmd] = true
    end
    for _, c in ipairs(FLOW_CMDS) do commands[c] = true end
    -- kag table aliases (registered handlers without contracts)
    local ok, kag = pcall(require, "kag")
    if ok and type(kag) == "table" then
        for k in pairs(kag) do
            if type(k) == "string" and not commands[k] then commands[k] = true end
        end
    end
    return commands
end

--- lsp.completion(lineText, pos) → array of completion items.
--  In-tag completion: after `[` suggests command names; inside a tag with
--  a command present suggests that command's params.
function lsp.completion(lineText)
    local items = {}
    -- command-name context: "[ch " -> params; "[c" -> commands
    local openBracket = lineText:find("[", 1, true)
    if not openBracket then return items end
    local afterBracket = lineText:sub(openBracket + 1)
    local cmdName = afterBracket:match("^%s*([%w_]+)")
    -- A command name only counts as "complete" when followed by a space
    -- or the closing bracket: "[ch" is still a name prefix, "[ch " is
    -- param completion.
    local afterName = afterBracket:match("^%s*[%w_]+(.*)$")
    local nameComplete = afterName ~= nil
        and (afterName:sub(1, 1) == " " or afterName:sub(1, 1) == "]")
    if not cmdName or not nameComplete then
        -- prefix of a command name: "[ch" or "[pla"
        local prefix = afterBracket:match("^%s*([%w_]*)")
        for cmd in pairs(ensure_index()) do
            if prefix == "" or cmd:find(prefix, 1, true) == 1 then
                local meta = schemaModule.meta(cmd)
                items[#items + 1] = {
                    label = cmd,
                    kind = KIND.Function,
                    detail = meta and meta.desc or "",
                    insertText = cmd .. " ",
                }
            end
        end
        return items
    end
    -- param context: "[ch " -> params of ch
    local specs = schemaModule.dumpContracts()[cmdName]
    if not specs then return items end
    -- prefix after the command name (leading space consumed)
    local prefix = (afterName or ""):match("^%s*([%w_]*)")
    for pname, spec in pairs(specs) do
        if pname ~= "_meta" and (prefix == "" or pname:find(prefix, 1, true) == 1) then
            local detail = (spec.type or "string")
                .. (spec.required and " (required)" or "")
                .. (spec.default ~= nil and (" = " .. tostring(spec.default)) or "")
            items[#items + 1] = {
                label = pname,
                kind = KIND.Field,
                detail = detail,
                insertText = pname .. "=",
            }
        end
    end
    return items
end

--- lsp.hover(cmd, param) → {title, text} describing the command contract
--  or the specific parameter (from _meta.desc + param spec).
function lsp.hover(cmd, param)
    if not cmd or cmd == "" then return nil end
    local contracts = schemaModule.dumpContracts()
    local specs = contracts[cmd]
    local meta = schemaModule.meta(cmd)
    if not specs and not meta then return nil end
    local title = "[" .. cmd .. "]"
    local lines = {}
    if meta and meta.desc and meta.desc ~= "" then
        lines[#lines + 1] = meta.desc
    end
    if param and specs and specs[param] then
        local spec = specs[param]
        local parts = { "param `" .. param .. "`" }
        if spec.type then parts[#parts + 1] = "type=" .. tostring(spec.type) end
        if spec.required then parts[#parts + 1] = "required" end
        if spec.default ~= nil then parts[#parts + 1] = "default=" .. tostring(spec.default) end
        if spec.min ~= nil then parts[#parts + 1] = "min=" .. tostring(spec.min) end
        if spec.max ~= nil then parts[#parts + 1] = "max=" .. tostring(spec.max) end
        if spec.choices then parts[#parts + 1] = "choices={" .. table.concat(spec.choices, "|") .. "}" end
        lines[#lines + 1] = table.concat(parts, "  ")
    elseif specs then
        local params = {}
        for pname in pairs(specs) do
            if pname ~= "_meta" then params[#params + 1] = pname end
        end
        table.sort(params)
        lines[#lines + 1] = "params: " .. table.concat(params, ", ")
    end
    if meta and meta.category then
        lines[#lines + 1] = "category: " .. tostring(meta.category)
            .. (meta.blocking and " (blocking)" or "")
    end
    return { title = title, text = table.concat(lines, "\n") }
end

--- lsp.diagnostics(text) → [{line, col, message, severity}] using the
--  tokenizer + contract validation (lightweight ks_check).
--  severity: 1 = error, 2 = warning (Monaco MarkerSeverity).
function lsp.diagnostics(text)
    local issues = {}
    if not text or text == "" then return issues end
    local ok, tokens = pcall(tokenizer.parse_with_offsets, text)
    if not ok or not tokens then return issues end

    -- line index for offset -> line mapping
    local lineStarts = { 1 }
    for i = 1, #text do
        if text:byte(i) == 10 then lineStarts[#lineStarts + 1] = i + 1 end
    end
    local function lineOf(offset)
        local lo, hi = 1, #lineStarts
        while lo < hi do
            local mid = (lo + hi + 1) // 2
            if lineStarts[mid] <= offset then lo = mid else hi = mid - 1 end
        end
        return lo
    end

    local contracts = schemaModule.dumpContracts()
    local function addIssue(line, col, message, severity)
        issues[#issues + 1] = {
            line = line, col = col or 1,
            message = message, severity = severity or 1,
        }
    end

    -- Truncation detection (ks_check parity): the offset stream can stop
    -- mid-scene on malformed input while earlier tokens parsed — flag it
    -- instead of reporting clean. Comments are consumed by the grammar's
    -- skip, so a scene ending in "; done" does not false-positive.
    local consumed = tokens[#tokens] and tokens[#tokens].end_offset or 0
    if consumed > 0 then
        local tail = text:sub(consumed + 1)
        while true do
            local stripped = tail:gsub("^%s*;[^\r\n]*", "")
            if stripped == tail then break end
            tail = stripped
        end
        local first = tail:find("%S")
        if first then
            addIssue(lineOf(consumed + first), 1,
                "parse stream stopped before end of input", 1)
        end
    end

    for _, tok in ipairs(tokens) do
        if tok.type == "command" then
            local cmd = tok.cmd
            local specs = contracts[cmd]
            -- Expression compile pre-check (ks_check parity): exp fields
            -- run through the KAG expression translator at runtime —
            -- validate they COMPILE so typos fail in the editor. [eval]/
            -- [emb] are STATEMENTS (assignments), so they get a plain
            -- chunk check (accept both return-wrapped and bare forms).
            do
                local exp = nil
                for _, pair in ipairs(tok.params or {}) do
                    if type(pair) == "table" and pair[1] == "exp" then
                        exp = pair[2]
                    end
                end
                if type(exp) == "string" and exp ~= "" then
                    local exprLang = require("kag.expr")
                    local fn = nil
                    if cmd == "eval" or cmd == "emb" then
                        fn = load("return " .. exp, "=ks_expr_check", "t", {})
                        if not fn then
                            fn = load(exp, "=ks_expr_check", "t", {})
                        end
                    else
                        fn = load("return " .. exprLang.translate(exp),
                            "=ks_expr_check", "t", {})
                    end
                    if not fn then
                        addIssue(lineOf(tok.offset), 1,
                            "expression in [" .. cmd .. "] does not compile: " .. exp, 1)
                    end
                end
            end
            if specs then
                local present = {}
                for _, pair in ipairs(tok.params or {}) do
                    if type(pair) == "table" and type(pair[1]) == "string" then
                        present[pair[1]] = true
                    end
                end
                -- required params
                for pname, spec in pairs(specs) do
                    if pname ~= "_meta" and spec.required and not present[pname] then
                        addIssue(lineOf(tok.offset), 1,
                            "[" .. cmd .. "] missing required param '" .. pname .. "'", 1)
                    end
                end
                -- any-of requirement (e.g. playbgm needs file OR storage)
                if specs._require_any then
                    local found = false
                    for _, name in ipairs(specs._require_any) do
                        if present[name] then found = true break end
                    end
                    if not found then
                        addIssue(lineOf(tok.offset), 1,
                            "[" .. cmd .. "] requires one of {"
                            .. table.concat(specs._require_any, ",") .. "}", 1)
                    end
                end
            else
                -- unknown command (not flow, not alias): warn like ks_check
                local known = ensure_index()[cmd]
                if not known then
                    addIssue(lineOf(tok.offset), 1,
                        "unknown KAG command '" .. cmd .. "' (will render as text)", 2)
                end
            end
        end
    end
    return issues
end

--- JSON string escape (shared by the json encoder).
local function json_str(s)
    s = tostring(s)
    return '"' .. s:gsub("\\", "\\\\"):gsub('"', '\\"')
        :gsub("\n", "\\n"):gsub("\r", "\\r") .. '"'
end

--- lsp.json(method, ...) → JSON string for the IDE (via /api/eval).
--  method: "completion" | "hover" | "diagnostics"
function lsp.json(method, ...)
    local result = nil
    if method == "completion" then
        result = lsp.completion(...)
    elseif method == "hover" then
        result = lsp.hover(...)
    elseif method == "diagnostics" then
        result = lsp.diagnostics(...)
    end
    if result == nil then result = {} end
    -- minimal JSON encoder for the result shapes (arrays of flat maps)
    local parts = {}
    if type(result) == "table" and result.title then
        -- hover: {title, text}
        parts[#parts + 1] = "{\"title\":" .. json_str(result.title)
            .. ",\"text\":" .. json_str(result.text) .. "}"
    else
        for _, item in ipairs(result) do
            local fields = {}
            for k, v in pairs(item) do
                if type(v) == "string" then
                    fields[#fields + 1] = json_str(k) .. ":" .. json_str(v)
                elseif type(v) == "number" then
                    fields[#fields + 1] = json_str(k) .. ":" .. tostring(v)
                end
            end
            parts[#parts + 1] = "{" .. table.concat(fields, ",") .. "}"
        end
    end
    return "[" .. table.concat(parts, ",") .. "]"
end

return lsp
