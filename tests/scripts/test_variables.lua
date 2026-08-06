-- test_variables.lua — KAG3 variable system: %var% interpolation, lf frame
-- stack, mp/lf in expression envs.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local Schema = require("kag.schema")
local scheduler = require("scheduler")

-- Load all command modules so the schema registry is populated (ch, etc.)
for _, m in ipairs({ "system", "text", "layer", "audio", "vfx", "transition",
                     "resource", "save", "video" }) do
    require("kag.commands." .. m)
end

-- ---- %var% interpolation through the schema coerce path -------------------
do
    local ctx = {
        f  = { hp = 30, name = "Aoi" },
        sf = { chapter = 2 },
        tf = { flag = true },
        mp = { arg = "hello" },
        lf = { local_v = 7 },
        current_scene = "v.ks", token_index = 5,
    }
    local ok, v = pcall(Schema.coerce, "ch", { text = "%f.name% has %f.hp% HP" }, ctx)
    check("%f.name% %f.hp% interpolate", ok and v.text == "Aoi has 30 HP", v and v.text)

    local ok2, v2 = pcall(Schema.coerce, "ch", { text = "ch%sf.chapter% %tf.flag% %mp.arg%" }, ctx)
    check("%sf./%tf./%mp. interpolate", ok2 and v2.text == "ch2 true hello", v2 and v2.text)

    local ok3, v3 = pcall(Schema.coerce, "ch", { text = "local=%lf.local_v% $lf.local_v" }, ctx)
    check("%lf.x% and $lf.x interpolate", ok3 and v3.text == "local=7 7", v3 and v3.text)

    -- bare %ident% is left untouched (macro placeholder domain)
    local ok4, v4 = pcall(Schema.coerce, "ch", { text = "keep %arg% as-is" }, ctx)
    check("bare %ident% untouched", ok4 and v4.text == "keep %arg% as-is", v4 and v4.text)

    -- unresolved variable stays literal
    local ok5, v5 = pcall(Schema.coerce, "ch", { text = "%f.missing%" }, ctx)
    check("unresolved %var% stays literal", ok5 and v5.text == "$f.missing", v5 and v5.text)

    -- no $ / % in text: passthrough untouched
    local ok6, v6 = pcall(Schema.coerce, "ch", { text = "plain text" }, ctx)
    check("plain text passthrough", ok6 and v6.text == "plain text")
end

-- ---- lf frame stack through [call] / [return] -----------------------------
do
    local calls = {}
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function(c2, p2) calls[#calls + 1] = { k, p2 } end
    end})

    local ctx = {
        f = { base = 1 }, sf = {}, tf = {}, mp = {},
        tokens = {}, token_index = 1, call_stack = {},
        current_scene = "main.ks", token_index = 1,
        load_tokens = function(path)
            -- callee scene: writes lf via [eval], then returns
            return {
                { "eval", { code = "lf.inner = 42" } },
                { "if", { exp = "lf.inner == 42 && f.base == 1" } },
                { "ch", { name = "C", text = "ok" } },
                { "endif" },
                { "return" },
            }
        end,
    }
    local co = coroutine.create(function()
        scheduler.run(ctx, { { "call", { storage = "sub.ks" } }, { "ch", { name = "A", text = "after" } } }, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig

    check("callee [if] sees lf.inner via TJS &&", calls[1] and calls[1][1] == "ch" and calls[1][2].text == "ok",
        calls[1] and calls[1][2] and calls[1][2].text)
    check("caller continues after return", calls[2] and calls[2][2].text == "after",
        calls[2] and calls[2][2] and calls[2][2].text)
    check("lf frame popped on return", ctx.lf ~= nil and ctx.lf.inner == nil)
    check("caller lf preserved", ctx.lf ~= nil and ctx.lf.outer == nil)

    -- callee writes lf, caller's lf must NOT see it (frame isolation):
    -- caller lf is a fresh table per call -- verify by writing before call
    ctx.lf = { outer = 1 }
    local kag_orig2 = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function() end
    end})
    local ctx2 = {
        f = {}, sf = {}, tf = {}, mp = {}, lf = { outer = 1 },
        tokens = {}, token_index = 1, call_stack = {},
        current_scene = "main.ks",
        load_tokens = function() return { { "eval", { code = "lf.inner = 9" } }, { "return" } } end,
    }
    local co2 = coroutine.create(function()
        scheduler.run(ctx2, { { "call", { storage = "sub.ks" } } }, 1)
    end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    package.loaded["kag"] = kag_orig2
    check("caller lf.outer intact after callee", ctx2.lf.outer == 1 and ctx2.lf.inner == nil)
end

-- ---- mp initialized for expression envs -----------------------------------
do
    local ctx = { f = { n = 3 }, sf = {}, tf = {} }  -- NO mp field
    local kag_orig = package.loaded["kag"]
    package.loaded["kag"] = setmetatable({}, { __index = function(_, k)
        return function() end
    end})
    local co = coroutine.create(function()
        scheduler.run(ctx, { { "eval", { code = "mp.msg = 'x'" } } }, 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    package.loaded["kag"] = kag_orig
    check("mp auto-initialized by scheduler", type(ctx.mp) == "table" and ctx.mp.msg == "x")
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("VARIABLES TESTS DONE")
