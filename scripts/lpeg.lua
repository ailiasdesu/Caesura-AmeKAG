-- =============================================================================
--  Caesura (AmeKAG) — lpeg.lua (pure-Lua PEG engine)
--  Covers the subset needed by tokenizer.lua: P, S, R, V, C, Ct, Cc, match
--  Pattern API: pat = lpeg.P"literal" / lpeg.S"set" / lpeg.R("a","z")
--               combined = pat1 * pat2  (sequence)
--               combined = pat1 + pat2  (ordered choice)
--               repeated = pat^0       (zero or more)
--               repeated = pat^1       (one or more)
--               repeated = pat^-1      (zero or more, same as ^0)
--               optional = -pat        (zero or one)
--               negated  = -lpeg.P(n)  (n == 1 means "not at end"; -1 means end-of-string)
--               captured = lpeg.C(pat) (string capture)
--               tablecap = lpeg.Ct(pat)(table capture)
--               constcap = lpeg.Cc(val)(constant capture)
--               grammar  = lpeg.P{ rule1 = ..., rule2 = ... }
--               lpeg.V("rule") inside grammar
-- =============================================================================

local lpeg = {}

-- ── Internal: Pattern metatable with operators ─────────────────────────────

local Pattern = {}
Pattern.__index = Pattern

function Pattern:__call(s, pos)
    return self.fn(self, s, pos or 1)
end

-- Sequence: a * b
function Pattern:__mul(other)
    local a, b = self, lpeg.P(other)
    return lpeg.P(function(s, pos)
        local r = { a(s, pos) }
        if not r[1] then return nil end
        local np = r[1]
        local r2 = { b(s, np) }
        if not r2[1] then return nil end
        -- merge captures
        local result = { r2[1] }
        for i = 2, #r do result[#result+1] = r[i] end
        for i = 2, #r2 do result[#result+1] = r2[i] end
        return unpack(result)
    end)
end

-- Ordered choice: a + b
function Pattern:__add(other)
    local a, b = self, lpeg.P(other)
    return lpeg.P(function(s, pos)
        local r = { a(s, pos) }
        if r[1] then return unpack(r) end
        return b(s, pos)
    end)
end

-- Repetition: pat^n
function Pattern:__pow(n)
    local pat = self
    if type(n) ~= "number" then error("__pow expects number") end
    if n >= 0 then  -- 0 or more / exactly n
        return lpeg.P(function(s, pos)
            local caps = {}
            local p = pos
            local count = 0
            while true do
                local r = { pat(s, p) }
                if not r[1] then break end
                p = r[1]
                for i = 2, #r do caps[#caps+1] = r[i] end
                count = count + 1
                if n > 0 and count >= n then break end
            end
            if n > 0 and count < n then return nil end
            local result = { p }
            for _, v in ipairs(caps) do result[#result+1] = v end
            return unpack(result)
        end)
    elseif n == -1 then  -- zero or more (same as ^0)
        return pat ^ 0
    else
        error("__pow: unsupported exponent " .. tostring(n))
    end
end

-- Unary minus: -pat = optional (zero or one)
function Pattern:__unm()
    local pat = self
    return lpeg.P(function(s, pos)
        local r = { pat(s, pos) }
        if r[1] then return unpack(r) end
        return { pos }
    end)
end

-- Subtraction: a - b = match a only if b fails at that position (negative lookahead for b)
function Pattern:__sub(other)
    if type(other) ~= "table" or getmetatable(other) ~= Pattern then
        -- Reverse dispatch: `1 - dquote` sends __sub(dquote, 1) in Lua
        return lpeg.P(other):__sub(self)
    end
    local a, b = self, other
    return lpeg.P(function(s, pos)
        local r2 = { b(s, pos) }
        if r2[1] then return nil end  -- b matched here, fail
        return a(s, pos)
    end)
end

-- ── Constructor ────────────────────────────────────────────────────────────

function lpeg.P(val)
    local t = type(val)
    if t == "string" then
        local n = #val
        if n == 0 then
            return setmetatable({ fn = function(_, s, pos) return { pos } end }, Pattern)
        end
        return setmetatable({ fn = function(_, s, pos)
            if s:sub(pos, pos + n - 1) == val then
                return { pos + n }
            end
            return nil
        end }, Pattern)
    elseif t == "function" then
        return setmetatable({ fn = val }, Pattern)
    elseif t == "table" then
        local mt = getmetatable(val)
        if mt == Pattern then return val end
        -- Grammar: { rule1 = ..., rule2 = ..., [1] = start_rule_name_or_index }
        -- Empty table: matches nothing (no-op). Used by Ct({}) for empty-table capture.
        local firstKey = next(val)
        if firstKey == nil then
            return setmetatable({ fn = function(_, s, pos) return { pos } end }, Pattern)
        end
        local grammar = {}
        local startKey = val[1] or 1
        -- Build all rules first
        for k, v in pairs(val) do
            if type(k) == "string" then
                grammar[k] = lpeg.P(v)
            end
        end
        -- Inject grammar table into every rule so V() lookups work
        for _, rule in pairs(grammar) do
            rule.grammar = grammar
        end
        local start = grammar[startKey]
        if not start then
            error("lpeg.P: grammar start rule '" .. tostring(startKey) .. "' not found")
        end
        return start
    elseif t == "number" then
        if val == -1 then  -- end of subject
            return setmetatable({ fn = function(_, s, pos)
                if pos > #s then return { pos } end
                return nil
            end }, Pattern)
        end
        if val == 1 then  -- match any single character
            return setmetatable({ fn = function(_, s, pos)
                if pos <= #s then return { pos + 1 } end
                return nil
            end }, Pattern)
        end
        error("lpeg.P: unexpected number " .. tostring(val))
    else
        error("lpeg.P: unsupported type " .. t)
    end
end

-- ── Set ────────────────────────────────────────────────────────────────────

function lpeg.S(set)
    return setmetatable({ fn = function(_, s, pos)
        local c = s:sub(pos, pos)
        if c ~= "" and set:find(c, 1, true) then
            return { pos + 1 }
        end
        return nil
    end }, Pattern)
end

-- ── Range ──────────────────────────────────────────────────────────────────

function lpeg.R(lo, hi)
    return setmetatable({ fn = function(_, s, pos)
        local c = s:sub(pos, pos)
        if c >= lo and c <= hi then
            return { pos + 1 }
        end
        return nil
    end }, Pattern)
end

-- ── String Capture ─────────────────────────────────────────────────────────

function lpeg.C(pat)
    local inner = lpeg.P(pat)
    return setmetatable({ fn = function(self, s, pos)
        local r = { inner(s, pos) }
        if not r[1] then return nil end
        local np = r[1]
        local captured = s:sub(pos, np - 1)
        return { np, captured }
    end }, Pattern)
end

-- ── Table Capture ──────────────────────────────────────────────────────────

function lpeg.Ct(pat)
    local inner = lpeg.P(pat)
    return setmetatable({ fn = function(self, s, pos)
        local r = { inner(s, pos) }
        if not r[1] then return nil end
        local np = r[1]
        local caps = {}
        for i = 2, #r do caps[#caps+1] = r[i] end
        return { np, caps }
    end }, Pattern)
end

-- ── Constant Capture ───────────────────────────────────────────────────────
-- Cc(val) always matches (consuming nothing) and captures val.

function lpeg.Cc(val)
    return setmetatable({ fn = function(self, s, pos)
        return { pos, val }
    end }, Pattern)
end

-- ── Grammar Variable ───────────────────────────────────────────────────────
-- V("name") looks up the named rule in the calling pattern's .grammar table.

function lpeg.V(name)
    local vpat = setmetatable({ vname = name }, Pattern)
    vpat.fn = function(self, s, pos)
        local g = rawget(self, "grammar")
        if not g then error("lpeg.V('" .. tostring(vpat.vname) .. "'): no grammar context") end
        local rule = g[vpat.vname]
        if not rule then error("lpeg.V: undefined rule '" .. tostring(vpat.vname) .. "'") end
        return rule(s, pos)
    end
    return vpat
end

-- ── Match ──────────────────────────────────────────────────────────────────

function lpeg.match(pattern, subject, init)
    init = init or 1
    local pat = lpeg.P(pattern)
    local r = { pat(subject, init) }
    if not r[1] then return nil end
    if #r == 1 then return r[1] end
    -- Return all captures
    local caps = {}
    for i = 2, #r do caps[#caps+1] = r[i] end
    if #caps == 1 then return caps[1] end
    return caps
end

return lpeg
