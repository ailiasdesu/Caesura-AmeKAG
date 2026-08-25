-- test_quickmenu.lua -- QuickMenu command binding tests
--
-- Sandbox discipline: this test performs NO writes to _G (scripts/sandbox.lua
-- rejects new globals, and rawset() bypasses are forbidden). Collaborators are
-- stubbed by assigning FIELDS on the already-loaded kag command table, which is
-- exactly the seam quickmenu.lua uses (it resolves require("kag") lazily).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path

local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local ok_kag, kag = pcall(require, "kag")
check("kag command table loads", ok_kag, tostring(kag))

-- Module resolution: in the engine, kag/init.lua preloads kag.quickmenu before
-- the sandbox locks require() down to package.loaded (scripts/sandbox.lua:186).
-- The suite runs this file AFTER test_sandbox, so the plain require would fail
-- with "not preloaded" even though the production wiring is correct. Compile the
-- real production file and register it exactly like the preload does -- no _G
-- writes, no rawset, and the code under test is the shipped file, not a copy.
-- (io.open under scripts/ and load() from a tests/ caller are both sandbox
-- allowlisted: scripts/sandbox.lua io.open allowlist + load_caller_is_trusted.)
local function load_quickmenu()
    if package.loaded["kag.quickmenu"] ~= nil then
        return true, package.loaded["kag.quickmenu"]
    end
    local ok_req, mod = pcall(require, "kag.quickmenu")
    if ok_req then return true, mod end
    local path = "scripts/kag/quickmenu.lua"
    local f, ferr = io.open(path, "r")
    if not f then return false, "cannot open " .. path .. ": " .. tostring(ferr) end
    local src = f:read("*a")
    f:close()
    -- _ENV must be passed EXPLICITLY: the sandbox load() wrapper
    -- (scripts/sandbox.lua:164) always forwards a 4th argument, so an omitted
    -- env arrives as an explicit nil and Lua sets the chunk's _ENV to nil
    -- ("attempt to index a nil value (upvalue '_ENV')" on the first require).
    local chunk, cerr = load(src, "@" .. path, "t", _G)
    if not chunk then return false, "compile failed: " .. tostring(cerr) end
    local ok_run, result = pcall(chunk)
    if not ok_run then return false, "module body failed: " .. tostring(result) end
    package.loaded["kag.quickmenu"] = result
    return true, result
end

local ok, qm = load_quickmenu()
check("quickmenu module loads", ok, tostring(qm))

if ok and ok_kag then
    -- Record the real handlers so each case restores them afterwards.
    local real_save, real_load, real_jump = kag.save, kag.load, kag.jump

    -- ---- quickmenu_auto -------------------------------------------------------
    do
        local ctx = { auto_mode = false }
        qm.quickmenu_auto(ctx, {})
        check("quickmenu_auto toggles on", ctx.auto_mode == true, tostring(ctx.auto_mode))
        qm.quickmenu_auto(ctx, {})
        check("quickmenu_auto toggles off", ctx.auto_mode == false, tostring(ctx.auto_mode))
        qm.quickmenu_auto(ctx, { mode = "on" })
        check("quickmenu_auto mode=on", ctx.auto_mode == true, tostring(ctx.auto_mode))
        qm.quickmenu_auto(ctx, { mode = "off" })
        check("quickmenu_auto mode=off", ctx.auto_mode == false, tostring(ctx.auto_mode))
        check("quickmenu_auto returns state", qm.quickmenu_auto(ctx, { mode = "on" }) == true, "expected true")
        -- No _toast global is required any more: a missing toast module must not
        -- turn the command into an error (the notification is best-effort).
        local okCall = pcall(qm.quickmenu_auto, { auto_mode = false }, {})
        check("quickmenu_auto survives without toast", okCall, "call errored")
    end

    -- ---- quickmenu_skip -------------------------------------------------------
    do
        local ctx1 = { skip_mode = false }
        qm.quickmenu_skip(ctx1, {})
        check("quickmenu_skip toggles on", ctx1.skip_mode == true, tostring(ctx1.skip_mode))
        qm.quickmenu_skip(ctx1, {})
        check("quickmenu_skip toggles off", ctx1.skip_mode == false, tostring(ctx1.skip_mode))

        local ctx2 = { skip_mode = false }
        qm.quickmenu_skip(ctx2, { mode = "seen" })
        check("quickmenu_skip mode=seen sets seen", ctx2.skip_mode == "seen", tostring(ctx2.skip_mode))
        qm.quickmenu_skip(ctx2, { mode = "seen" })
        check("quickmenu_skip mode=seen toggles off", ctx2.skip_mode == false, tostring(ctx2.skip_mode))
    end

    -- ---- quickmenu_log --------------------------------------------------------
    do
        local ctx = { backlog_visible = false }
        qm.quickmenu_log(ctx, {})
        check("quickmenu_log sets backlog_visible", ctx.backlog_visible == true, tostring(ctx.backlog_visible))
    end

    -- ---- quickmenu_config -----------------------------------------------------
    do
        local ctx = { config_open = false }
        qm.quickmenu_config(ctx, {})
        check("quickmenu_config sets config_open", ctx.config_open == true, tostring(ctx.config_open))
    end

    -- ---- quickmenu_qsave: routes to the registered [save] handler -------------
    do
        local seen = nil
        kag.save = function(c, params)
            seen = { ctx = c, slot = params and params.slot }
            c.tf = c.tf or {}
            c.tf.save_result = "ok"
        end
        local ctx = {}
        local saved = qm.quickmenu_qsave(ctx, { slot = 3 })
        check("quickmenu_qsave calls [save]", seen ~= nil, "handler not called")
        check("quickmenu_qsave forwards slot", seen and seen.slot == 3, tostring(seen and seen.slot))
        check("quickmenu_qsave reports ok", saved == true, tostring(saved))
        check("quickmenu_qsave leaves save_result", ctx.tf and ctx.tf.save_result == "ok",
              tostring(ctx.tf and ctx.tf.save_result))

        -- Failure must be reported honestly: [save] signals errors through
        -- ctx.tf.save_result, NEVER through its return value (always nil).
        kag.save = function(c, _params)
            c.tf = c.tf or {}
            c.tf.save_result = "error"
        end
        local ctx2 = {}
        check("quickmenu_qsave reports failure", qm.quickmenu_qsave(ctx2, { slot = 1 }) == false,
              "expected false on save_result=error")

        -- A throwing [save] must not escape the command.
        kag.save = function() error("boom") end
        local ctx3 = {}
        local okCall, res = pcall(qm.quickmenu_qsave, ctx3, { slot = 2 })
        check("quickmenu_qsave contains handler error", okCall and res == false, tostring(res))

        -- Slot clamping (schema allows 0..99; direct callers may exceed it).
        kag.save = function(c, params)
            seen = { slot = params and params.slot }
            c.tf = c.tf or {}
            c.tf.save_result = "ok"
        end
        qm.quickmenu_qsave({}, { slot = 250 })
        check("quickmenu_qsave clamps high slot", seen and seen.slot == 99, tostring(seen and seen.slot))
        qm.quickmenu_qsave({}, { slot = -5 })
        check("quickmenu_qsave clamps negative slot", seen and seen.slot == 0, tostring(seen and seen.slot))
        qm.quickmenu_qsave({}, {})
        check("quickmenu_qsave defaults to slot 0", seen and seen.slot == 0, tostring(seen and seen.slot))

        kag.save = real_save
    end

    -- ---- quickmenu_qload: routes to the registered [load] handler -------------
    do
        local seen = nil
        kag.load = function(c, params)
            seen = { slot = params and params.slot }
            c.tf = c.tf or {}
            c.tf.load_result = "ok"
        end
        check("quickmenu_qload reports ok", qm.quickmenu_qload({}, { slot = 7 }) == true, "expected true")
        check("quickmenu_qload forwards slot", seen and seen.slot == 7, tostring(seen and seen.slot))

        kag.load = function(c, _params)
            c.tf = c.tf or {}
            c.tf.load_result = "error"
        end
        check("quickmenu_qload reports failure", qm.quickmenu_qload({}, { slot = 0 }) == false,
              "expected false on load_result=error")

        kag.load = real_load
    end

    -- ---- quickmenu_title: delegates to kag.jump ------------------------------
    do
        local seen = nil
        kag.jump = function(_c, target) seen = target end
        check("quickmenu_title returns true", qm.quickmenu_title({}, {}) == true, "expected true")
        check("quickmenu_title defaults to title label", seen == "title", tostring(seen))
        qm.quickmenu_title({}, { scene = "chapter2" })
        check("quickmenu_title forwards scene", seen == "chapter2", tostring(seen))
        kag.jump = real_jump
    end

    -- ---- Registration into the kag command table -----------------------------
    check("quickmenu_auto registered in kag", type(kag.quickmenu_auto) == "function", type(kag.quickmenu_auto))
    check("quickmenu_skip registered in kag", type(kag.quickmenu_skip) == "function", type(kag.quickmenu_skip))
    check("quickmenu_log registered in kag",  type(kag.quickmenu_log) == "function",  type(kag.quickmenu_log))
    check("quickmenu_qsave registered in kag", type(kag.quickmenu_qsave) == "function", type(kag.quickmenu_qsave))
    check("quickmenu_qload registered in kag", type(kag.quickmenu_qload) == "function", type(kag.quickmenu_qload))
    check("quickmenu_title registered in kag", type(kag.quickmenu_title) == "function", type(kag.quickmenu_title))
    check("quickmenu_config registered in kag", type(kag.quickmenu_config) == "function", type(kag.quickmenu_config))

    -- ---- Schema contracts ----------------------------------------------------
    do
        local ok_schema, schema = pcall(require, "kag.schema")
        check("schema module available", ok_schema, tostring(schema))
        if ok_schema and type(schema.specs) == "function" then
            local spec = schema.specs("quickmenu_qsave")
            check("quickmenu_qsave schema registered", type(spec) == "table", tostring(spec))
            if type(spec) == "table" then
                check("quickmenu_qsave schema has slot", spec.slot ~= nil, "missing slot spec")
            end
            check("quickmenu_auto schema registered", type(schema.specs("quickmenu_auto")) == "table",
                  tostring(schema.specs("quickmenu_auto")))
        end
    end
end

-- Summary
local passed = 0
for _, r in ipairs(results) do if r then passed = passed + 1 end end
print(string.format("Results: %d/%d passed", passed, #results))
if passed < #results then os.exit(1) end
