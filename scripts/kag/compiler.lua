-- =============================================================================
--  Caesura (AmeKAG) — kag/compiler.lua
--  KAG Neo-Genesis compile-time front-end (Phase A of the core rewrite).
--
--  Turns a token stream (tokenizer.parse output, either record format
--  {type=,cmd=,params=} or array format {cmd, params}) into a COMPILED
--  instruction stream: the same array plus a `_compiled` side table with
--  everything the scheduler would otherwise recompute per token at
--  runtime:
--
--    _compiled.flow     [i] = { kind, jump|target, ... }  -- O(1) jumps,
--                          no runtime skip_to scans
--    _compiled.exprs    [i] = translated Lua source        -- TJS->Lua done
--                          once at compile time (load still happens at
--                          runtime with the ctx env, cached there)
--    _compiled.params   [i] = normalized param table       -- {{k,v}} pairs
--                          resolved to {k=v} once
--    _compiled.handlers [i] = handler function reference   -- kag[cmd] lookup
--                          resolved once
--    _compiled.labels   name -> token index                -- label index
--
--  The scheduler runs compiled streams via the fast path (checks
--  tokens._compiled); hand-built token arrays (tests, macro splices)
--  still run the legacy path unchanged. `compile` is a pure function of
--  the token stream -- no ctx, no I/O, deterministic.
-- =============================================================================

local compiler = {}

local schemaModule = require("kag.schema")
local exprLang = require("kag.expr")

-- Flow commands the scheduler handles inline (mirror of scheduler.lua's
-- flow_commands table -- kept here so the compiler can classify tokens).
local FLOW = {
    ["if"] = true, ["else"] = true, ["elseif"] = true, ["endif"] = true,
    ["switch"] = true, ["case"] = true, ["default"] = true,
    ["endswitch"] = true,
    ["jump"] = true, ["call"] = true, ["return"] = true,
    ["link"] = true, ["end"] = true,
    ["label"] = true,
    ["macro"] = true, ["endmacro"] = true, ["erasemacro"] = true,
    ["eval"] = true, ["emb"] = true,
    ["iscript"] = true,
    ["while"] = true, ["endwhile"] = true,
    ["for"] = true, ["endfor"] = true,
    ["break"] = true, ["continue"] = true,
    ["stop"] = true,
}

-- Tokens whose `exp`/`expr`/`cond`/`start`/`end`/`step` params carry
-- expressions that get compiled once (TJS->Lua translation).
local EXPR_TOKENS = {
    ["if"] = "exp", ["elseif"] = "exp", ["while"] = "exp",
    ["for"] = nil,  -- handled specially below (three numeric exprs)
    ["switch"] = nil,  -- switch expr is a variable name, not TJS (KAG3 form)
}

--- Normalize one token to array format {cmd, params} (params = {} when
--  absent). Accepts both record and array input formats.
local function to_array_tok(tok)
    if type(tok) ~= "table" then return nil end
    if tok.type then
        local cmd = tok.cmd or tok.type
        if tok.type == "label" then
            return { "label", { name = tok.name } }
        elseif tok.type == "text" then
            return { "ch", { text = tok.content or "" } }
        elseif tok.type == "iscript" then
            return { "iscript", { body = tok.body or "" } }
        end
        return { cmd, tok.params or {} }
    end
    return { tok[1], tok[2] or {} }
end

--- Normalize raw param pairs {{key,val},...} into {key=val} (bare
--  positional args keep numeric keys 1..N, matching scheduler semantics).
--  Hand-built streams may already use named tables ({text="a"}) -- those
--  have no pair array part and pass through unchanged (identity, so
--  handler mutation semantics match the legacy runtime path exactly).
local function normalize_params(raw)
    if type(raw) ~= "table" then return {} end
    local out = {}
    local has_pairs = false
    for _, p in ipairs(raw) do
        if type(p) == "table" and type(p[1]) == "string" then
            has_pairs = true
            local key = tonumber(p[1]) or p[1]
            out[key] = p[2]
        end
    end
    if not has_pairs then return raw end  -- named table: keep identity
    return out
end

--- Compile-time expression translation (TJS->Lua) for one param value.
--  Returns the translated source string (or the original when translation
--  is not applicable). Runtime still loads it with the ctx env; the
--  scheduler's expr cache keys on translated source so the load is
--  amortized. Compile time removes the per-token translate() cost.
local function compile_expr_param(tok_cmd, params)
    if tok_cmd == "for" then
        for _, pname in ipairs({ "start", "end", "step" }) do
            local v = params[pname]
            if type(v) == "string" then
                params[pname] = exprLang.translate(v)
            end
        end
        return
    end
    local pname = EXPR_TOKENS[tok_cmd]
    if pname and type(params[pname]) == "string" then
        params[pname] = exprLang.translate(params[pname])
    end
end

--- Build the label index (first definition wins, KAG3 semantics).
local function build_labels(tokens)
    local labels = {}
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and type(tok[2]) == "table"
            and type(tok[2].name) == "string" then
            if not labels[tok[2].name] then labels[tok[2].name] = i end
        end
    end
    return labels
end

-- ---------------------------------------------------------------------------
-- Flow pre-scan: compute O(1) jump targets for every flow token.
-- Semantics must EXACTLY match the scheduler's runtime skip_to scans
-- (see scheduler.lua): depth-aware nesting, closers vs branch markers.
-- ---------------------------------------------------------------------------

-- Skip target: index of the token AFTER the matching closer (i.e. where
-- execution continues). Returns #tokens+1 when the chain is unterminated.
local function scan_skip(tokens, start, targets, opens, closers)
    closers = closers or targets
    local depth = 1
    for i = start + 1, #tokens do
        local cmd = tokens[i][1]
        if targets[cmd] then
            if depth == 1 then return i end
            if closers[cmd] then depth = depth - 1 end
        elseif opens and opens[cmd] then
            depth = depth + 1
        end
    end
    return #tokens + 1
end

local IF_CHAIN = { ["elseif"] = true, ["else"] = true, ["endif"] = true }
local IF_CLOSER = { ["endif"] = true }
local IF_OPENS = { ["if"] = true }
local WHILE_SET = { ["endwhile"] = true }
local WHILE_OPENS = { ["while"] = true }
local FOR_SET = { ["endfor"] = true }
local FOR_OPENS = { ["for"] = true }
local LOOP_SET = { ["endwhile"] = true, ["endfor"] = true }
local LOOP_OPENS = { ["while"] = true, ["for"] = true }
local SWITCH_SET = { ["case"] = true, ["default"] = true, ["endswitch"] = true }
local SWITCH_CLOSER = { ["endswitch"] = true }
local SWITCH_OPENS = { ["switch"] = true }
local ENDSWITCH_SET = { ["endswitch"] = true }
local ENDSWITCH_OPENS = { ["switch"] = true }

--- Compile the flow metadata for a token array. Returns:
--   flow[i] = {
--     kind = "if"|"elseif"|"else"|"endif"|"while"|"endwhile"|"for"|
--            "endfor"|"break"|"continue"|"switch"|"case"|"default"|
--            "endswitch"|"jump"|"call"|"return"|"link"|"label"|"end",
--     ...kind-specific fields (jump targets as token indexes)
--   }
local function compile_flow(tokens)
    local flow = {}
    local n = #tokens

    for i = 1, n do
        local cmd = tokens[i][1]
        if cmd == "if" then
            -- false branch: jump to first elseif/else/endif (not consumed)
            local t = scan_skip(tokens, i, IF_CHAIN, IF_OPENS, IF_CLOSER)
            flow[i] = { kind = "if", jump_false = t }
        elseif cmd == "elseif" or cmd == "else" then
            -- taken branch already handled: skip rest of chain
            local t = scan_skip(tokens, i, IF_CHAIN, IF_OPENS, IF_CLOSER)
            flow[i] = { kind = cmd, jump_chain_end = t }
        elseif cmd == "endif" then
            flow[i] = { kind = "endif" }
        elseif cmd == "while" then
            local t = scan_skip(tokens, i, WHILE_SET, WHILE_OPENS, WHILE_SET)
            flow[i] = { kind = "while", skip_body_to = t }
        elseif cmd == "endwhile" then
            flow[i] = { kind = "endwhile" }
        elseif cmd == "for" then
            local t = scan_skip(tokens, i, FOR_SET, FOR_OPENS, FOR_SET)
            flow[i] = { kind = "for", skip_body_to = t }
        elseif cmd == "endfor" then
            flow[i] = { kind = "endfor" }
        elseif cmd == "break" or cmd == "continue" then
            local t = scan_skip(tokens, i, LOOP_SET, LOOP_OPENS, LOOP_SET)
            flow[i] = { kind = cmd, loop_end = t }
        elseif cmd == "switch" then
            -- Compile-time case table: { [case_value] = body_start_idx }
            -- for depth-1 cases only (a nested switch's cases belong to
            -- IT -- the runtime scan's depth-awareness is compiled away).
            -- default maps to the special key "default".
            local cases, default_idx, end_idx = {}, nil, nil
            local depth = 1
            for j = i + 1, n do
                local scmd = tokens[j][1]
                if scmd == "switch" then
                    depth = depth + 1
                elseif scmd == "endswitch" then
                    if depth == 1 then end_idx = j break end
                    depth = depth - 1
                elseif depth == 1 and scmd == "case" then
                    local caseVal = (tokens[j][2] or {})[1] or ""
                    cases[tostring(caseVal)] = j + 1
                elseif depth == 1 and scmd == "default" then
                    if not default_idx then default_idx = j + 1 end
                end
            end
            flow[i] = {
                kind = "switch",
                cases = cases, default = default_idx,
                endswitch = end_idx or (n + 1),
            }
        elseif cmd == "case" or cmd == "default" then
            -- when a previous case was taken: skip to endswitch
            local t = scan_skip(tokens, i, ENDSWITCH_SET, ENDSWITCH_OPENS,
                                ENDSWITCH_SET)
            flow[i] = { kind = cmd, skip_to_end = t }
        elseif cmd == "endswitch" then
            flow[i] = { kind = "endswitch" }
        elseif cmd == "jump" or cmd == "call" or cmd == "link" then
            -- compile-time label resolution for intra-scene targets
            local params = tokens[i][2] or {}
            local target = params.target or params.label or params.storage
            if type(params[1]) == "string" then target = target or params[1] end
            flow[i] = { kind = cmd, target = target }
        elseif cmd == "return" or cmd == "end" or cmd == "stop" then
            flow[i] = { kind = cmd }
        elseif cmd == "label" then
            flow[i] = { kind = "label" }
        end
    end
    return flow
end

--- compiler.compile(tokens) → tokens (same array) with tokens._compiled set.
--  Pure, deterministic, idempotent (recompiling a compiled stream is a no-op
--  when the side table is already present and complete).
function compiler.compile(tokens)
    if not tokens or #tokens == 0 then return tokens end
    if tokens._compiled then return tokens end

    -- 1) Normalize to array format + normalized params + handler binding.
    local norm = {}
    local handlers = {}
    local params_by_idx = {}
    local exprs = {}
    local kag = nil  -- lazy: only needed when a handler lookup is required
    for i, tok in ipairs(tokens) do
        local at = to_array_tok(tok)
        if not at then
            norm[i] = tok  -- passthrough (unknown shape)
        else
            norm[i] = at
            local cmd = at[1]
            if FLOW[cmd] then
                -- flow tokens: normalize params for the flow branches that
                -- read them (if/while/for/jump/call/link/switch/macro/eval)
                local p = normalize_params(at[2])
                at[2] = p
                params_by_idx[i] = p
                compile_expr_param(cmd, p)
                if cmd == "if" or cmd == "elseif" or cmd == "while"
                    or cmd == "for" then
                    -- keep the translated source for the scheduler (it
                    -- loads with the ctx env at runtime, cached there)
                    local pname = (cmd == "for") and "start" or EXPR_TOKENS[cmd]
                    if pname and type(p[pname]) == "string" then
                        exprs[i] = p[pname]
                    end
                end
            else
                -- regular command: bind the handler once
                if not kag then kag = require("kag") end
                local handler = kag[cmd]
                if handler then handlers[i] = handler end
                local p = normalize_params(at[2])
                at[2] = p
                params_by_idx[i] = p
            end
        end
    end
    for i = 1, #norm do tokens[i] = norm[i] end

    -- 2) Flow pre-scan.
    local flow = compile_flow(tokens)

    -- 3) Label index (compile-time; scheduler no longer rescans).
    local labels = build_labels(tokens)

    tokens._compiled = {
        flow = flow,
        exprs = exprs,
        params = params_by_idx,
        handlers = handlers,
        labels = labels,
    }
    return tokens
end

--- compiler.isCompiled(tokens) → boolean
function compiler.isCompiled(tokens)
    return type(tokens) == "table" and tokens._compiled ~= nil
end

--- compiler.invalidate(tokens) — drop the side table (after a macro
--  splice mutates the stream; the stream must be recompiled).
function compiler.invalidate(tokens)
    if type(tokens) == "table" then tokens._compiled = nil end
end

return compiler
