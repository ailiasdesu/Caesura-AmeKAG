-- Real HistoryUI coroutine: closing its scope must release modal ownership.
local passed, failed = 0, 0
local function check(name, value)
    if value then passed = passed + 1; print("PASS " .. name)
    else failed = failed + 1; print("FAIL " .. name) end
end

local function fixture()
    local nodes, focuses = {}, {}
    local env = setmetatable({}, {__index = _G})
    env._G = env
    local ctx = {input_focus = "kag", backlog = {{text="observed dialogue", scene="A.ks", token_index=1}}}
    local backend = {
        create_solid_texture = function() return 1 end,
        render_text = function() if env.render_error then error("history render failure") end end,
        set_input_focus = function(value) focuses[#focuses + 1] = value end,
    }
    local modules = {
        backend = backend,
        viewport = {wh = function() return 1280, 720 end},
        layers = {
            ensure = function(_, name) nodes[name] = nodes[name] or {}; return nodes[name] end,
            get = function(_, name) return nodes[name] end,
            get_layer = function(name) return nodes[name] end,
        },
    }
    env.require = function(name) return assert(modules[name], name) end
    local file = assert(io.open("scripts/history_ui.lua", "rb"))
    local source = file:read("*a"); file:close()
    if source:sub(1,3) == "\239\187\191" then source = source:sub(4) end
    local history = assert(load(source, "@scripts/history_ui.lua", "t", env))()
    local co = coroutine.create(function() return history.show(ctx) end)
    return env, ctx, nodes, focuses, co
end

local function hidden(nodes)
    for _, node in pairs(nodes) do if node.visible then return false end end
    return true
end

for _, path in ipairs({"close", "escape", "jump", "error"}) do
    local env, ctx, nodes, focuses, co = fixture()
    assert(coroutine.resume(co))
    check(path .. " opened real history", ctx.input_focus == "history" and not hidden(nodes))
    if path == "close" then
        assert(coroutine.close(co))
    elseif path == "escape" then
        env._GAME_KEY_ESC = true
        assert(coroutine.resume(co))
    elseif path == "jump" then
        env._GAME_KEY_ENTER = true
        local ok, result = coroutine.resume(co)
        check("jump result preserved", ok and result and result.scene == "A.ks" and result.index == 1)
    else
        env.render_error = true
        local ok = coroutine.resume(co)
        check("history render error is visible", not ok)
        coroutine.close(co)
    end
    check(path .. " restores focus", ctx.input_focus == "kag" and focuses[#focuses] == "KAG")
    check(path .. " hides every owned layer", hidden(nodes))
    local count = #focuses
    coroutine.close(co)
    check(path .. " cleanup occurs once", #focuses == count)
end

print(string.format("HISTORY CLEANUP TESTS: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
