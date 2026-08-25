-- =============================================================================
--  Caesura (AmeKAG) — kag/determinism.lua
--  Deterministic scene execution for tests (Battle 3a): drive a .ks scene
--  through the scheduler in pure Lua (no engine/GPU), capture the ctx
--  variable state at checkpoints, and compare runs for determinism.
--
--  This is the foundation of the deterministic-engineering generation gap:
--  any scene can be executed headlessly and its state asserted, so game
--  logic is testable without a GPU and regressions are caught before CI.
-- =============================================================================

local determinism = {}

local scheduler = require("scheduler")
local tokenizer = require("tokenizer")
local compiler = require("kag.compiler")

-- ---------------------------------------------------------------------------
-- Mock backend: the KAG commands dispatch through the kag table, but the
-- audio/layer/text handlers touch the `backend` module — provide a
-- no-op mock so scenes run headlessly. Tests may override via
-- determinism.set_mock(overrides).
-- ---------------------------------------------------------------------------

local default_mock = {
    create_viewport = function() return 1 end,
    create_solid_texture = function() return { _mock = true } end,
    render_text = function() end,
    load_texture = function() return { _mock = true } end,
    draw_viewport = function() end,
    audio_play = function() end,
    audio_stop = function() end,
    audio_set_volume = function() end,
    clear_text = function() end,
    get_input_focus = function() return "kag" end,
    set_text_style = function() end,
    get_text_size = function() return 100, 20 end,
    show_textbox = function() end,
    hide_textbox = function() end,
    particles_create_emitter = function() return 1 end,
    particles_update_emitter = function() end,
    particles_destroy_emitter = function() end,
    video_play = function() end,
    video_stop = function() end,
}

local mock_backend = nil
local mock_kag = nil
local saved_backend = nil
local saved_kag = nil

--- determinism.install_mocks(overrides) — replace the backend/kag modules
--  with no-op mocks so scenes run headlessly. Call before run_scene.
function determinism.install_mocks(overrides)
    if not mock_backend then
        saved_backend = package.loaded["backend"]
        saved_kag = package.loaded["kag"]
    end
    mock_backend = {}
    for k, v in pairs(default_mock) do mock_backend[k] = v end
    if overrides then
        for k, v in pairs(overrides) do mock_backend[k] = v end
    end
    package.loaded["backend"] = mock_backend
    package.loaded["kag"] = mock_kag or package.loaded["kag"]
    -- kag commands require the backend at call time; ensure modules that
    -- captured the real backend get the mock (reload command modules)
    -- (commands read require("backend") lazily in handlers, so the
    -- package.loaded swap above is enough)
end

--- determinism.restore_mocks() — put the real modules back.
function determinism.restore_mocks()
    if saved_backend then package.loaded["backend"] = saved_backend end
    if saved_kag then package.loaded["kag"] = saved_kag end
    mock_backend = nil
end

--- determinism.run_scene(source, opts) → {ctx, steps, trace}
--  Executes a .ks source (or token array) through the scheduler with a
--  mock kag table. opts:
--    max_steps   (default 10000) — safety cap for runaway scripts
--    dt          (default 16)    — ms fed to each resume
--    kag_override               — extra kag handler mocks
--  Returns the ctx (with f/sf/tf/backlog/token_index) plus a trace of
--  dispatched commands for assertions.
function determinism.run_scene(source, opts)
    opts = opts or {}
    local tokens = source
    if type(source) == "string" then
        tokens = tokenizer.parse(source)
    end

    -- kag mock: every known command becomes a no-op that records its
    -- name; handlers that must behave (ch -> backlog) are special-cased.
    -- The kag table MUST be swapped BEFORE compile so compiler.compile
    -- binds the mock handlers (compiled.handlers) instead of the real
    -- command implementations.
    local trace = {}
    local kag_table = {}
    local function record(cmd)
        return function(ctx, params)
            trace[#trace + 1] = { cmd = cmd, params = params }
            -- minimal state behavior shared by commands
            if cmd == "ch" or cmd == "text" then
                local text = (params and (params.text or params[1])) or ""
                ctx.backlog = ctx.backlog or {}
                ctx.backlog[#ctx.backlog + 1] = text
            elseif cmd == "set" then
                -- [set f.x 100] / [set f.x = 100] (real logic for if/while)
                local var = params.var or params[1]
                local val = params.value or params[2]
                if type(var) == "string" and var:match("^f%.") and val ~= nil then
                    ctx.f = ctx.f or {}
                    ctx.f[var:sub(3)] = tonumber(val) or val
                end
            elseif cmd == "inc" then
                local var = params.var or params[1]
                local by = tonumber(params.value or params[2]) or 1
                if type(var) == "string" and var:match("^f%.") then
                    ctx.f = ctx.f or {}
                    local key = var:sub(3)
                    ctx.f[key] = (tonumber(ctx.f[key]) or 0) + by
                end
            end
        end
    end
    -- collect known commands from the schema + flow set
    local known = {}
    local schema = require("kag.schema")
    for cmd in pairs(schema.dumpContracts()) do known[cmd] = true end
    for _, c in ipairs({ "if", "elseif", "else", "endif", "while", "endwhile",
        "for", "endfor", "break", "continue", "jump", "call", "return",
        "link", "label", "macro", "endmacro", "erasemacro", "switch",
        "case", "default", "endswitch", "eval", "emb", "iscript",
        "endscript", "select", "sel", "endselect", "end", "stop" }) do
        known[c] = true
    end
    for cmd in pairs(known) do kag_table[cmd] = record(cmd) end
    if opts.kag_override then
        for k, v in pairs(opts.kag_override) do kag_table[k] = v end
    end
    local saved = package.loaded["kag"]
    package.loaded["kag"] = kag_table

    -- Force recompile: a previously compiled token array carries handlers
    -- bound to the REAL command table — invalidate so compile binds the
    -- mock handlers.
    compiler.invalidate(tokens)
    compiler.compile(tokens)

    local ctx = {
        f = {}, sf = {}, tf = {}, mp = {}, lf = {},
        tokens = tokens, token_index = 1, call_stack = {},
        layers = {}, backlog = {}, macros = {},
        stop_flag = false, load_tokens = function() end,
        current_scene = opts.scene or "determinism_test.ks",
    }
    local co = coroutine.create(function()
        scheduler.run(ctx, tokens, 1)
    end)
    local steps = 0
    local max = opts.max_steps or 10000
    while coroutine.status(co) == "suspended" and steps < max do
        local ok, err = coroutine.resume(co, opts.dt or 16)
        if not ok then
            ctx._error = err
            break
        end
        steps = steps + 1
    end
    if steps >= max and coroutine.status(co) == "suspended" then
        ctx._timed_out = true
    end
    package.loaded["kag"] = saved
    return ctx, steps, trace
end

--- determinism.state_snapshot(ctx) → plain data copy of the assertion
--  surface (f/sf/tf/mp/lf/backlog/token_index/current_scene). JSON-safe.
function determinism.state_snapshot(ctx)
    local function copy(t)
        if type(t) ~= "table" then return t end
        local out = {}
        for k, v in pairs(t) do out[k] = copy(v) end
        return out
    end
    return {
        f = copy(ctx.f or {}),
        sf = copy(ctx.sf or {}),
        tf = copy(ctx.tf or {}),
        mp = copy(ctx.mp or {}),
        lf = copy(ctx.lf or {}),
        backlog = copy(ctx.backlog or {}),
        token_index = ctx.token_index,
        current_scene = ctx.current_scene,
    }
end

--- determinism.assert_state(actual, expected, label) → true/false with a
--  diff message; deep-compares the snapshot surfaces.
function determinism.assert_state(actual, expected, label)
    if not expected then return true end
    local function deep_eq(a, b)
        if type(a) ~= type(b) then return false end
        if type(a) ~= "table" then return a == b end
        for k, v in pairs(a) do
            if not deep_eq(v, b and b[k]) then return false end
        end
        for k in pairs(b or {}) do
            if a[k] == nil then return false end
        end
        return true
    end
    local ok = deep_eq(actual, expected)
    if not ok and label then
        print("[determinism] state mismatch at " .. label)
    end
    return ok
end

return determinism
