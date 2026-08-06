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

-- Neo-Genesis: label index. One pass per scene build (at load) turns
-- [jump *label] from O(n) scan into O(1) lookup. The index is rebuilt
-- whenever a macro splice mutates the stream (see invalidate below).
-- Exported for tests and tooling (ks_check could reuse it); the
-- scheduler itself calls the local.
-- Scene-path allowlist for cross-scene jumps (security: jump/call/link
-- concatenate target onto assets/script/ and tokenizer.parse_file does
-- io.open -- a crafted target like ../evil.ks would traverse out of the
-- game dir and EXECUTE an arbitrary .ks as a scene. Same predicate
-- family as SaveCommands._safeScenePath; kept local to avoid a
-- dependency cycle (scheduler is the bottom of the stack).
local function is_safe_scene_path(path)
    if type(path) ~= "string" or #path == 0 then return false end
    if path:find("..", 1, true) then return false end
    if path:find("^assets/script/") ~= 1 then return false end
    return path:find("%.ks$") ~= nil
end

local function build_label_index(tokens)
    local idx = {}
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and tok[2] and type(tok[2].name) == "string" then
            idx[tok[2].name] = idx[tok[2].name] or i  -- first wins (KAG3)
        end
    end
    return idx
end

local function find_label(tokens, name, label_index)
    if label_index and label_index[name] then
        return label_index[name]
    end
    for i, tok in ipairs(tokens) do
        if tok[1] == "label" and tok[2] and tok[2].name == name then
            return i
        end
    end
    return nil
end

-- Shared [if]/[elseif]/[while]/[for] expression evaluation.
-- Neo-Genesis: delegates to kag/expr.lua, which translates TJS-style
-- operators (&& || ! != ?:) into Lua and reports compile/runtime errors
-- visibly (scene:line) instead of silently taking the else branch.
local exprLang = require("kag.expr")
local function eval_expr(ctx, expr)
    return exprLang.evaluate(ctx, expr)
end


local function skip_to(tokens, start_idx, targets, closers)
    -- closers: which target commands CLOSE the enclosing chain (only
    -- endif does; elseif/else are branch markers and must not decrement
    -- the depth -- otherwise an outer-false [if] lands on an inner
    -- chain's endif and double-executes the outer body).
    closers = closers or targets
    local depth = 1
    for i = start_idx + 1, #tokens do
        local cmd = tokens[i][1]
        if targets[cmd] then
            if depth == 1 then return i end
            if closers[cmd] then depth = depth - 1 end
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
            elseif t.type == "text" then
                -- Bare dialogue line (KAG3 style): [ch text="..."]
                tokens[j] = { "ch", { text = t.content or "" } }
            elseif t.type == "iscript" then
                tokens[j] = { "iscript", { body = t.body or "" } }
            else
                tokens[j] = { t.cmd or t.type, t.params or {} }
                end
            end
        end
    end
    -- Lazy label index AFTER normalization: build_label_index expects the
    -- array format ({"label", {name=...}}), not the tokenizer's raw
    -- records -- building too early produced a sticky EMPTY index.
    if ctx and ctx.label_index == nil then
        ctx.label_index = build_label_index(tokens)
    end
    -- if/elseif chain state: each entry tracks whether a branch was TAKEN
    -- so later elseif/else in the SAME chain are skipped (nested chains
    -- are independent via the stack).
    local if_stack = {}
    -- switch/case state: taken case bodies must not fall through into
    -- later cases (KAG3 semantics -- a matched case runs alone).
    local switch_stack = {}
    -- while state. Bounded -- a runaway loop in a script (a variable
    -- that never changes) must not hang the runner. KAG3 had no bound;
    -- Neo-Genesis caps iterations and errors loudly.
    local while_stack = {}
    -- for-loop state (numeric counter loops, same bounded regime).
    local for_stack = {}
    local WHILE_MAX_ITERS = 65536

    -- Normalize params: convert array-format {{key,val},...} to named access
    -- so commands can use params.name, params.target, etc.
    for j, t in ipairs(tokens) do
        local params = t[2]
        if type(params) == "table" then
            for _, p in ipairs(params) do
                if type(p) == "table" and type(p[1]) == "string" then
                    -- numeric-string keys ("1") become NUMBER keys so
                    -- handlers read params[1] (bare KAG3 positional args)
                    local key = tonumber(p[1]) or p[1]
                    params[key] = p[2]
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
            -- bare [jump next.ks] -> params[1] (KAG3 syntax); STRING-only:
            -- with named params (even a typo), params[1] is the raw pair
            -- table and target:sub would throw (review should-fix)
            local target = params.target or params.label or params.storage
            if type(params[1]) == "string" then target = target or params[1] end
            if not target then
                print("[WARN] [jump] missing target/label/storage parameter")
            elseif target:sub(1,1) ~= "*" then
                -- Cross-scene jump: load new scene file (bare [jump x.ks]
                -- and named target= both land here -- audit fix)
                local path = "assets/script/" .. target
                if not is_safe_scene_path(path) then
                    print("[WARN] [jump] blocked scene path: " .. path)
                else
                    local new_tokens = ctx.load_tokens and ctx.load_tokens(path)
                    if new_tokens then
                        ctx.tokens = new_tokens
                        ctx.token_index = 1
                        ctx.current_scene = path
                        ctx.call_stack = {}
                        ctx.layers = {}
                        ctx.backlog = {}
                        ctx.label_index = nil  -- raw tokens: entry lazy-builds
                        operation.cancel_all(ctx)
                        return
                    else
                        print("[WARN] [jump] failed to load scene: " .. path)
                    end
                end
            else
                -- Intra-scene jump: find label (target may be "label" or "*label")
                local label = target:gsub("^*", "")  -- strip leading * if present
                local idx = find_label(tokens, label, ctx.label_index)
                if idx then
                    i = idx
                else
                    print("[WARN] [jump] label not found: " .. label)
                end
            end

        -- Flow control: [call]
        elseif cmd == "call" then
            -- bare [call next.ks] -> params[1] (string-only guard)
            local target = params.target or params.storage
            if type(params[1]) == "string" then target = target or params[1] end
            if not target then
                print("[WARN] [call] missing target/storage parameter")
            else
            local path = "assets/script/" .. target
            local new_tokens = nil
            if not is_safe_scene_path(path) then
                print("[WARN] [call] blocked scene path: " .. path)
            else
                new_tokens = ctx.load_tokens and ctx.load_tokens(path)
            end
            if new_tokens then
                ctx.call_stack = ctx.call_stack or {}
                table.insert(ctx.call_stack, {
                    tokens = tokens, index = i + 1,
                    label_index = ctx.label_index,
                    scene = ctx.current_scene,
                })
                tokens = new_tokens
                ctx.tokens = tokens
                ctx.current_scene = path
                ctx.label_index = nil  -- raw tokens: run() entry rebuilds
                i = 0
            end
            end

        -- Flow control: [return]
        elseif cmd == "return" then
            ctx.call_stack = ctx.call_stack or {}  -- mirror the [call] guard
            local frame = table.remove(ctx.call_stack)
            if frame then
                ctx.label_index = frame.label_index  -- caller scene again
                ctx.current_scene = frame.scene       -- caller scene NAME again
            end
            if frame then
                tokens = frame.tokens
                ctx.tokens = tokens
                i = frame.index - 1
            else
                return  -- No call stack, end execution
            end

        -- Flow control: [link]
        elseif cmd == "link" then
            -- (index rebuilt below with the swapped stream)
            -- bare [link next.ks] -> params[1] (string-only guard)
            local target = params.target or params.storage
            if type(params[1]) == "string" then target = target or params[1] end
            if not target then
                -- review should-fix: a typo'd [link] must NOT wipe
                -- layers/backlog/call_stack -- WARN and keep scene state
                print("[WARN] [link] missing target/storage parameter")
            else
            local path = "assets/script/" .. target
            local new_tokens = nil
            if not is_safe_scene_path(path) then
                print("[WARN] [link] blocked scene path: " .. path)
            else
                -- security info: clear ONLY after the allowlist accepts
                -- (a blocked traversal link must not wipe scene state)
                ctx.layers = {}
                ctx.backlog = {}
                operation.cancel_all(ctx)
                ctx.call_stack = {}
                new_tokens = ctx.load_tokens and ctx.load_tokens(path)
            end
            if new_tokens then
                tokens = new_tokens
                ctx.tokens = tokens
                ctx.token_index = 1
                ctx.current_scene = path
                ctx.label_index = nil  -- raw tokens: run() entry rebuilds
                i = 0
            end
            end

        -- Flow control: [end]
        elseif cmd == "end" then
            return

        -- Flow control: [label] ?? no-op, used by jump/call
        elseif cmd == "label" then
            -- pass

        -- Flow control: [if]/[else]/[endif]
        elseif cmd == "if" then
            local ok, result = eval_expr(ctx, params.exp or "false")
            local taken = ok and result or false
            if_stack[#if_stack + 1] = taken
            if not taken then
                -- Skip to the next elseif/else/endif; the target is NOT
                -- consumed (i-1) so an elseif evaluates its own expression.
                i = skip_to(tokens, i, {
                    ["elseif"] = true, ["else"] = true, ["endif"] = true,
                    opens = {["if"] = true}
                }, {["endif"] = true}) - 1
            end

        elseif cmd == "elseif" then
            local taken = if_stack[#if_stack]
            if taken then
                -- A previous branch was taken: skip the rest of the chain.
                i = skip_to(tokens, i, {
                    ["elseif"] = true, ["else"] = true, ["endif"] = true,
                    opens = {["if"] = true}
                }, {["endif"] = true}) - 1
            else
                local ok, result = eval_expr(ctx, params.exp or "false")
                taken = ok and result or false
                if_stack[#if_stack] = taken
                if not taken then
                    i = skip_to(tokens, i, {
                        ["elseif"] = true, ["else"] = true, ["endif"] = true,
                        opens = {["if"] = true}
                    }, {["endif"] = true}) - 1
                end
            end

        elseif cmd == "else" then
            local taken = if_stack[#if_stack]
            if taken then
                i = skip_to(tokens, i, {
                    ["endif"] = true,
                    opens = {["if"] = true}
                }) - 1  -- do not consume: endif must pop the chain
            end
            -- (not taken: execute the else body -- fall through)

        elseif cmd == "endif" then
            if_stack[#if_stack] = nil  -- pop the chain

        
        -- Flow control: [while]/[endwhile] -- bounded data-driven loops.
        elseif cmd == "while" then
            -- Total-iteration guard, PER SCENE: entries are popped each
            -- iteration, so the bound must live outside the stack; and a
            -- session-global counter would permanently disable [while]
            -- after 65k total executions across scenes (security LOW).
            -- Keying by scene lets each scene budget its own 65k.
            local wscene = ctx.current_scene or "?"
            ctx._whileIterByScene = ctx._whileIterByScene or {}
            ctx._whileIterByScene[wscene] =
                (ctx._whileIterByScene[wscene] or 0) + 1
            if ctx._whileIterByScene[wscene] > WHILE_MAX_ITERS then
                error("[while] exceeded " .. WHILE_MAX_ITERS
                    .. " total iterations in scene '" .. wscene
                    .. "' (bounded loop guard)", 0)
            end
            local ok, result = eval_expr(ctx, params.exp or "false")
            -- ALWAYS push (ended flag): the matching endwhile must know
            -- whether the loop is still live (rewind) or was skipped
            -- because the condition turned false (pop and continue).
            while_stack[#while_stack + 1] = {
                pos = i,  -- loop head: endwhile rewinds to i-1
                ended = not (ok and result),
            }
            if not (ok and result) then
                -- Skip the body: depth-aware (nested while has its own
                -- endwhile). i stops on the endwhile so it pops.
                i = skip_to(tokens, i, {
                    ["endwhile"] = true,
                    opens = {["while"] = true}
                }, {["endwhile"] = true}) - 1
            end

        elseif cmd == "endwhile" then
            local w = while_stack[#while_stack]
            if w then
                if w.ended then
                    while_stack[#while_stack] = nil  -- loop over: pop
                else
                    -- POP before rewinding: the next [while] re-pushes a
                    -- fresh entry. Without this, a live loop's entry stays
                    -- on the stack and the ENDWHILE of an OUTER loop grabs
                    -- the stale inner entry (pos) and rewinds into the
                    -- inner body -- an infinite inner re-run.
                    -- (The per-entry iters guard was removed: ctx's total
                    -- iteration guard always fires first -- dead code.)
                    while_stack[#while_stack] = nil
                    i = w.pos - 1  -- loop head: re-evaluate next iteration
                end
            end

        -- Flow control: [for var="i" start="0" end="3" step="1"]...[endfor]
        -- numeric loops (Neo-Genesis; KAG3 had no counter loop). Shares
        -- the while per-scene guard: a step=0 loop cannot hang the runner.
        elseif cmd == "for" then
            -- Shared total-iteration guard (per scene, same as [while]).
            local wscene = ctx.current_scene or "?"
            ctx._whileIterByScene = ctx._whileIterByScene or {}
            ctx._whileIterByScene[wscene] =
                (ctx._whileIterByScene[wscene] or 0) + 1
            if ctx._whileIterByScene[wscene] > WHILE_MAX_ITERS then
                error("[for] exceeded " .. WHILE_MAX_ITERS
                    .. " total iterations in scene '" .. wscene
                    .. "' (bounded loop guard)", 0)
            end
            local vname = params.var or "i"
            local ok0, sval = eval_expr(ctx, tostring(params.start or "0"))
            local ok1, eval = eval_expr(ctx, tostring(params["end"] or "0"))
            local ok2, sstep = eval_expr(ctx, tostring(params.step or "1"))
            if ok0 and ok1 and ok2 then
                local sv = tonumber(sval) or 0
                local ev = tonumber(eval) or 0
                local sp = tonumber(sstep) or 1
                if sp == 0 then sp = 1 end  -- degenerate step: treat as 1
                ctx.f = ctx.f or {}
                -- FIRST entry sets the counter; a re-entry (endfor
                -- rewound here) must KEEP the incremented value or the
                -- loop restarts from start forever. Same-named nested
                -- loops are a script anti-pattern (documented).
                ctx._forStackMarks = ctx._forStackMarks or {}
                if not ctx._forStackMarks[vname] then
                    ctx.f[vname] = sv
                end
                for_stack[#for_stack + 1] = {
                    var = vname, endv = ev, step = sp, pos = i,
                    ended = not ((sp > 0 and sv <= ev) or (sp < 0 and sv >= ev)),
                }
                ctx._forStackMarks[vname] = true
                if for_stack[#for_stack].ended then
                    -- start already past the end: skip the body
                    i = skip_to(tokens, i, {
                        ["endfor"] = true,
                        opens = {["for"] = true}
                    }, {["endfor"] = true}) - 1
                end
            else
                -- bad numbers: skip the body (evaluated loudly already)
                i = skip_to(tokens, i, {
                    ["endfor"] = true,
                    opens = {["for"] = true}
                }, {["endfor"] = true}) - 1
            end

        elseif cmd == "endfor" then
            local w = for_stack[#for_stack]
            if w then
                if w.ended then
                    for_stack[#for_stack] = nil  -- loop over: pop
                    -- An empty/skipped loop set the mark too: clear it or
                    -- a later same-name [for] reuses the stale counter
                    -- (review should-fix).
                    if ctx._forStackMarks then
                        ctx._forStackMarks[w.var] = nil
                    end
                else
                    local cur = tonumber(ctx.f and ctx.f[w.var]) or 0
                    cur = cur + w.step
                    ctx.f[w.var] = cur
                    local over = (w.step > 0 and cur > w.endv)
                        or (w.step < 0 and cur < w.endv)
                    if over then
                        for_stack[#for_stack] = nil  -- done: pop
                        if ctx._forStackMarks then
                            ctx._forStackMarks[w.var] = nil
                        end
                    else
                        for_stack[#for_stack] = nil  -- pop; next [for] re-pushes
                        i = w.pos - 1  -- loop head
                    end
                end
            end

        -- Flow control: [break]/[continue] inside while/for loops
        -- (Neo-Genesis; KAG3 had neither). break pops the innermost loop
        -- and jumps past its end token; continue skips the rest of the
        -- body so the end token runs (endfor increments / endwhile
        -- re-evaluates).
        elseif cmd == "break" or cmd == "continue" then
            local wl = while_stack[#while_stack]
            local fl = for_stack[#for_stack]
            local wpos = wl and wl.pos or -1
            local fpos = fl and fl.pos or -1
            if wpos < 0 and fpos < 0 then
                error("[" .. cmd .. "] outside a loop", 0)
            end
            -- the innermost loop is the one whose head token is LAST
            local inWhile = wpos > fpos
            local tgt = {
                ["endwhile"] = true, ["endfor"] = true,
                opens = {["while"] = true, ["for"] = true}
            }
            local cls = {["endwhile"] = true, ["endfor"] = true}
            if cmd == "break" then
                -- pop the loop we are leaving; its end token must NOT run
                -- (it would pop again / rewind into the body)
                if inWhile then
                    while_stack[#while_stack] = nil
                else
                    if ctx._forStackMarks then
                        ctx._forStackMarks[fl.var] = nil
                    end
                    for_stack[#for_stack] = nil
                end
                i = skip_to(tokens, i, tgt, cls)  -- +1 skips the end token
            else  -- continue
                -- skip to the end token; -1 lets the loop process it
                i = skip_to(tokens, i, tgt, cls) - 1
            end

        -- Flow control: [switch]/[case]/[default]/[endswitch] (spec [1.4])
        elseif cmd == "switch" then
            -- switch/case dispatch (Alpha: flat only, single-level)
            local switchExpr = params[1] or ""
            local switchVal = nil
            -- Script variables live in ctx.f (the eval env, shared with
            -- [if]/[while]); ctx.variables is the legacy binding-layer
            -- table. f wins, variables is the fallback.
            if ctx.f and ctx.f[switchExpr] ~= nil then
                switchVal = tostring(ctx.f[switchExpr])
            elseif ctx.variables and ctx.variables[switchExpr] ~= nil then
                switchVal = tostring(ctx.variables[switchExpr])
            else
                switchVal = switchExpr
            end

            -- Scan forward to find matching case. Depth-aware: a nested
            -- switch's case tokens belong to IT, not us -- only depth-1
            -- cases match (review note: the flat scan landed on a nested
            -- switch's matching case inside a no-match outer body).
            local caseStart = nil
            local scanIdx = i + 1  -- start AFTER our own token
            local scanDepth = 1
            while scanIdx <= #tokens do
                local stok = tokens[scanIdx]
                local scmd = stok[1]
                if scmd == "switch" then
                    scanDepth = scanDepth + 1
                elseif scmd == "endswitch" then
                    if scanDepth == 1 then break end  -- OUR endswitch
                    scanDepth = scanDepth - 1
                elseif scanDepth == 1 and scmd == "case" then
                    local caseParams = stok[2] or {}
                    local caseVal = caseParams[1] or ""
                    if tostring(caseVal) == switchVal then
                        caseStart = scanIdx + 1
                        break
                    end
                elseif scanDepth == 1 and scmd == "default" then
                    caseStart = scanIdx + 1
                    break
                end
                scanIdx = scanIdx + 1
            end

            -- ALWAYS push (taken flag): the matching endswitch pops OUR
            -- entry -- a no-match switch that skipped its body must not
            -- let its endswitch pop the OUTER switch's entry.
            if caseStart then
                switch_stack[#switch_stack + 1] = true
                i = caseStart - 1  -- loop's i+1 lands ON the first body token
            else
                switch_stack[#switch_stack + 1] = false  -- no case taken
                -- Skip to OUR endswitch, depth-aware (a nested switch in
                -- the skipped body has its own endswitch).
                i = skip_to(tokens, i, {
                    ["endswitch"] = true,
                    opens = {["switch"] = true}
                }, {["endswitch"] = true}) - 1
            end

        elseif cmd == "case" or cmd == "default" then
            if switch_stack[#switch_stack] then
                -- A case was already taken: this case body must NOT run
                -- (no fall-through). Skip to OUR endswitch (depth-aware --
                -- a nested switch inside the skipped case body has its
                -- own endswitch; stopping at the nearest one would pop
                -- the wrong entry and resume the skipped body), leaving
                -- i ON the endswitch so the loop processes it and POPS.
                i = skip_to(tokens, i, {
                    ["endswitch"] = true,
                    opens = {["switch"] = true}
                }, {["endswitch"] = true}) - 1
            end
            -- (not taken: this case belongs to a switch whose case body
            -- is currently executing -- fall through and run its body)

        elseif cmd == "endswitch" then
            switch_stack[#switch_stack] = nil  -- pop the switch

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
            -- Whitelist env (audit): the old `__index = _G` exposed the
            -- ENTIRE global table -- os.execute and friends reachable from
            -- a [eval] tag -- while the strict-sandbox path (SystemCommands
            -- via sandbox.execute) never ran because this branch intercepts
            -- first. Same whitelist as [iscript]: os restricted to
            -- clock/date/time, no require/debug/io/load.
            local env = {
                ctx       = ctx,
                f         = ctx.f or {},
                sf        = ctx.sf or {},
                tf        = ctx.tf or {},
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
            }
            local fn, compileErr = load(code, "=eval", "t", env)
            if fn then
                local ok, result = pcall(fn)
                if ok and result ~= nil then
                    ctx.tf = ctx.tf or {}
                    rawset(ctx.tf, "eval_result", result)  -- no-trap invariant
                end
            else
                print(string.format("[eval] Compile error @ %s:%d: %s",
                    ctx.current_scene or "?", ctx.token_index or 0,
                    tostring(compileErr)))
            end

        -- Flow control: [macro] / [endmacro]
        elseif cmd == "macro" then
            -- bare [macro m args=...] -> params[1] (KAG3 syntax: the
            -- macro name is a positional arg -- audit fix); string-only
            -- guard (named-only params leave a pair table at params[1])
            local name = params.name
            if type(name) ~= "string" and type(params[1]) == "string" then
                name = params[1]
            end
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
                -- Neo-Genesis: parameterized macros (KAG3 args feature).
                -- args="who,what" declares the params; invocations fill
                -- %who% / %what% placeholders in the body.
                ctx.macro_args = ctx.macro_args or {}
                local args = {}
                if type(params.args) == "string" then
                    for a in params.args:gmatch("[^,]+") do
                        a = a:match("^%s*(.-)%s*$")  -- trim
                        if #a > 0 then args[#args + 1] = a end
                    end
                end
                ctx.macro_args[name] = args
            end

        elseif cmd == "erasemacro" then
            -- bare [erasemacro m] -> params[1] (string-only guard)
            local name = params.name
            if type(name) ~= "string" and type(params[1]) == "string" then
                name = params[1]
            end
            if name and ctx.macros then
                ctx.macros[name] = nil
                if ctx.macro_args then ctx.macro_args[name] = nil end
            end
        elseif cmd == "endmacro" then
            -- pass (handled by macro recording above)

        -- Regular command: dispatch to kag table
        else
            -- Check if it's a macro invocation
            local macro_body = ctx.macros and ctx.macros[cmd]
            if macro_body then
                -- Expansion budget (audit): a self-recursive macro
                -- ([macro m][m][endmacro]) would splice its body forever,
                -- growing the token array until the C++ instruction budget
                -- aborts -- an explicit per-scene cap fails fast instead.
                ctx._macroExpansions = (ctx._macroExpansions or 0) + 1
                if ctx._macroExpansions > 1000 then
                    error("[macro] expansion budget exceeded (possible "
                          .. "self-recursive macro at " .. cmd .. ")")
                end
                -- Parameter substitution: build a per-invocation copy with
                -- %arg% placeholders filled from the call's params. The
                -- shared body is never mutated (multiple calls reuse it).
                local argNames = ctx.macro_args and ctx.macro_args[cmd]
                -- Deep-copy helper (splice scope: no-arg macros use it).
                local function deepCopy(t)
                    if type(t) ~= "table" then return t end
                    local out = {}
                    for k, v in pairs(t) do out[k] = deepCopy(v) end
                    return out
                end
                if argNames and #argNames > 0 then
                    -- Substitute %arg% placeholders inside the params
                    -- table (token t[2] is ALWAYS the params table -- the
                    -- string content lives in params values).
                    local function fill(v)
                        if type(v) == "string" then
                            return (v:gsub("%%([%w_]+)%%", function(an)
                                -- numeric placeholders (%1%) map to the
                                -- bare positional key params[1] (audit:
                                -- scheduler normalizes "1" to a NUMBER
                                -- key, so params["1"] was always nil)
                                local key = tonumber(an) or an
                                -- string-only: a mismatched arg usage
                                -- leaves the raw pair table at params[1]
                                -- and gsub would raise on a table value
                                -- (review nit -- keep the literal instead)
                                local val = params[key]
                                if type(val) == "string" then
                                    return val
                                end
                                return ("%" .. an .. "%")
                            end))
                        elseif type(v) == "table" then
                            -- Deep COPY: the body params table is shared by
                            -- every invocation -- in-place substitution
                            -- would poison later calls (review caught this).
                            local out = {}
                            for k, vv in pairs(v) do out[k] = fill(vv) end
                            return out
                        end
                        return v
                    end
                    local sub = {}
                    for n = 1, #macro_body do
                        local t = macro_body[n]
                        sub[n] = { t[1], fill(t[2]) }
                    end
                    macro_body = sub
                end
                -- Splice macro body into token stream, replacing the invocation.
                -- Copy the body (shallow, per-token) and shift the tail in place
                -- to avoid rebuilding the whole array with 3 inserts per call.
                local tailStart = i + 1
                local tailCount = #tokens - tailStart + 1
                local bodyCount = #macro_body
                -- Grow the array by bodyCount - 1 (the invocation is replaced
                -- by bodyCount tokens).
                local grow = bodyCount - 1
                if grow > 0 then
                    for _ = 1, grow do table.insert(tokens, nil) end
                    table.move(tokens, tailStart, #tokens - grow, tailStart + grow)
                elseif grow < 0 then
                    table.move(tokens, tailStart, #tokens, tailStart + grow)
                    for _ = 1, -grow do table.remove(tokens) end
                end
                for n = 1, bodyCount do
                    -- Deep-copy every token: no-arg macros share the body's
                    -- params tables by reference today (security info item)
                    -- and a handler mutating them would poison later calls.
                    tokens[i - 1 + n] = {macro_body[n][1], deepCopy(macro_body[n][2])}
                end
                ctx.tokens = tokens
                ctx.label_index = nil  -- splice changed the stream: next jumps re-scan
                i = i - 1  -- will point to first body token after i = i + 1
            else
                -- text chunks become [ch] commands
                -- Neo-Genesis rules: coerce typed params BEFORE dispatch so
                -- handlers get numbers/booleans and bad input is reported
                -- with location instead of silently swallowed.
                local schema = require("kag.schema")
                if schema.isMigrated(cmd) then
                    params = schema.coerce(cmd, params, ctx)
                end
                local handler = kag[cmd]
                local actual_cmd = cmd
                if not handler and type(cmd) == "string" and #cmd > 0 then
                    -- Unrecognized text ?? treat as [ch] -- through the ch
                    -- contract so interpolation ($f.name) and type coercion
                    -- apply to plain dialogue lines too (the main use case).
                    handler = kag["ch"]
                    if handler then
                        params = {text = cmd}
                        actual_cmd = "ch"
                        if schema.isMigrated("ch") then
                            params = schema.coerce("ch", params, ctx)
                        end
                    end
                end
                if handler then
                    local status, err = pcall(handler, ctx, params)
                    if not status then
                        -- Lua-side error reporting
                        print("[ERROR] KAG command '" .. actual_cmd .. "' failed: " .. tostring(err))
                        if ctx.handle_error then
                            pcall(ctx.handle_error, actual_cmd, tostring(err), i)
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

scheduler.build_label_index = build_label_index
scheduler.is_safe_scene_path = is_safe_scene_path
scheduler.find_label = find_label
return scheduler
