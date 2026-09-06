-- Runtime-menu cancellation requires fresh module instances bound to mock
-- backends. Run in the orphan subprocess suite: the main suite deliberately
-- locks require() to package.loaded after its sandbox verification.
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local Save = require("kag.commands.save")
local passed, failed = 0, 0
local function check(name, condition)
    if condition then
        passed = passed + 1
        print("PASS " .. name)
    else
        failed = failed + 1
        print("FAIL " .. name)
    end
end

-- settings cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"settings", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"settings", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects = "GAME", 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["settings"] = nil
    local menu = require("settings")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}
    ctx.settingsValues = {volume_bgm = 17}
    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("settings cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("settings exposes active focus", ctx.input_focus == "settings")
    _G._GAME_KEY_ENTER = true
    check("settings coroutine close succeeds", coroutine.close(co))
    check("settings restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("settings clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("settings removes operation", #(ctx.active_operations or {}) == 0)
    check("cancel does not apply settings", effects == 0)
    local again = coroutine.create(function() menu.show(ctx) end)
    check("settings reopens after cancel", coroutine.resume(again) and coroutine.status(again) == "suspended")
    coroutine.close(again)
    for id in ipairs(textures) do
        check("settings cancellation released texture " .. id, destroyed[id] == 1)
    end
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("settings normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("settings normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("settings normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("settings normal close removes operation", #(ctx.active_operations or {}) == 0)
    check("settings normal close applies values", effects > 0)
    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("settings token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("settings resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("settings token cancel adds no normal side effect", effects == beforeCancel)
    check("settings token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    local retired = coroutine.create(function() menu.show(ctx) end)
    check("settings retired fixture opens", coroutine.resume(retired))
    require("kag.operation").cancel_all(ctx)
    local replacement = coroutine.create(function() menu.show(ctx) end)
    check("settings replacement opens", coroutine.resume(replacement))
    local beforeRetired = effects
    check("retired settings coroutine exits", coroutine.resume(retired)
        and coroutine.status(retired) == "dead")
    check("retired settings does not close replacement", ctx._settingsActive == true
        and ctx.input_focus == "settings" and effects == beforeRetired)
    coroutine.close(replacement)
    check("settings repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("settings hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("settings releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

-- gallery cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"gallery", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"gallery", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects = "GAME", 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["gallery"] = nil
    local menu = require("gallery")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}
    menu.scan = function() return {{id="one",name="One",path="one.png"}} end
    ctx.unlockedCG = {one=true}
    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("gallery cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("gallery exposes active focus", ctx.input_focus == "gallery")
    _G._GAME_KEY_ENTER = true
    check("gallery coroutine close succeeds", coroutine.close(co))
    check("gallery restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("gallery clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("gallery removes operation", #(ctx.active_operations or {}) == 0)
    check("cancel clears gallery state", ctx.galleryState == nil)
    for id in ipairs(textures) do
        check("gallery cancellation released texture " .. id, destroyed[id] == 1)
    end
    menu._cleanupTextures()
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("gallery normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("gallery normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("gallery normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("gallery normal close removes operation", #(ctx.active_operations or {}) == 0)

    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("gallery token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("gallery resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("gallery token cancel adds no normal side effect", effects == beforeCancel)
    check("gallery token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("gallery repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("gallery hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("gallery releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

-- music_room cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"music_room", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"music_room", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects = "GAME", 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["music_room"] = nil
    local menu = require("music_room")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}
    menu.scan = function() return {{id="one",name="One",path="one.ogg"}} end
    menu._loadFavorites = function() end
    ctx.unlockedMusic = {one=true}
    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("music_room cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("music_room exposes active focus", ctx.input_focus == "music_room")
    _G._GAME_KEY_ENTER = true
    check("music_room coroutine close succeeds", coroutine.close(co))
    check("music_room restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("music_room clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("music_room removes operation", #(ctx.active_operations or {}) == 0)
    check("cancel does not start audio", effects == 0)
    for id in ipairs(textures) do
        check("music_room cancellation released texture " .. id, destroyed[id] == 1)
    end
    menu._cleanupTextures()
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("music_room normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("music_room normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("music_room normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("music_room normal close removes operation", #(ctx.active_operations or {}) == 0)

    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("music_room token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("music_room resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("music_room token cancel adds no normal side effect", effects == beforeCancel)
    check("music_room token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    -- Cancellation must stop an owned preview without replaying or fading it in.
    local stops, playCount = {}, 0
    package.loaded["audio"].play_bgm = function() playCount = playCount + 1 end
    package.loaded["audio"].stop_bgm = function(fade) stops[#stops + 1] = fade end
    local preview = coroutine.create(function() menu.show(ctx) end)
    check("preview cancel fixture opens", coroutine.resume(preview))
    menu.play("one")
    check("preview cancellation closes", coroutine.close(preview))
    menu.stop()
    check("preview cancellation stops once immediately", #stops == 1 and stops[1] == 0)
    check("preview cancellation never restarts audio", playCount == 1)
    local normalPreview = coroutine.create(function() menu.show(ctx) end)
    check("normal preview fixture opens", coroutine.resume(normalPreview))
    menu.play("one")
    _G._GAME_KEY_ESC = true
    check("normal preview exit completes", coroutine.resume(normalPreview)
        and coroutine.status(normalPreview) == "dead")
    check("normal preview exit keeps existing playback", #stops == 1 and playCount == 2)
    menu.stop()
    check("music_room repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("music_room hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("music_room releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

-- saveload_menu cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"saveload_menu", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"saveload_menu", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects = "GAME", 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["saveload_menu"] = nil
    local menu = require("saveload_menu")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}

    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("saveload_menu cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("saveload_menu exposes active focus", ctx.input_focus == "saveload")
    _G._GAME_KEY_ENTER = true
    check("saveload_menu coroutine close succeeds", coroutine.close(co))
    check("saveload_menu restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("saveload_menu clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("saveload_menu removes operation", #(ctx.active_operations or {}) == 0)

    for id in ipairs(textures) do
        check("saveload_menu cancellation released texture " .. id, destroyed[id] == 1)
    end
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("saveload_menu normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("saveload_menu normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("saveload_menu normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("saveload_menu normal close removes operation", #(ctx.active_operations or {}) == 0)

    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("saveload_menu token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("saveload_menu resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("saveload_menu token cancel adds no normal side effect", effects == beforeCancel)
    check("saveload_menu token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    -- The real handler saves/loads only after show() releases its UI scope.
    local savedKAG = _G.KAG
    local savedCalls, loadedCalls = 0, 0
    _G.KAG = {
        list_saves = function() return {} end,
        save_game = function()
            savedCalls = savedCalls + 1
            return true
        end,
        load_game = function()
            loadedCalls = loadedCalls + 1
            return nil
        end,
    }
    for _, mode in ipairs({"save", "load"}) do
        local handlerCtx = {input_focus="kag", f={}, sf={}, tf={}, mp={}, variables={},
            current_scene="s.ks", token_index=1, _executing_command="saveload"}
        local handler = coroutine.create(function() Save.saveload(handlerCtx, {mode=mode}) end)
        check("saveload " .. mode .. " handler opens", coroutine.resume(handler))
        local saveable, reason = pcall(require("kag.transient_state").assert_saveable, handlerCtx)
        check("saveload " .. mode .. " visible menu rejects capture",
            not saveable and tostring(reason):find("runtime menu", 1, true) ~= nil)
        _G._GAME_KEY_ENTER = true
        check("saveload " .. mode .. " handler completes", coroutine.resume(handler)
            and coroutine.status(handler) == "dead")
        check("saveload " .. mode .. " handler restores saveable focus",
            handlerCtx.input_focus == "kag" and #(handlerCtx.active_operations or {}) == 0)
    end
    check("saveload confirmed save reaches real save binding", savedCalls == 1)
    check("saveload confirmed load reaches real load binding", loadedCalls == 1)
    _G.KAG = savedKAG
    check("saveload_menu repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("saveload_menu hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("saveload_menu releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

-- title_menu cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"title_menu", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"title_menu", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects, renders = "GAME", 0, 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() renders = renders + 1 end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["title_menu"] = nil
    local menu = require("title_menu")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}

    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("title_menu cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("title_menu exposes active focus", ctx.input_focus == "title")
    _G._GAME_KEY_ENTER = true
    check("title_menu coroutine close succeeds", coroutine.close(co))
    check("title_menu restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("title_menu clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("title_menu removes operation", #(ctx.active_operations or {}) == 0)

    for id in ipairs(textures) do
        check("title_menu cancellation released texture " .. id, destroyed[id] == 1)
    end
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("title_menu normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("title_menu normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("title_menu normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("title_menu normal close removes operation", #(ctx.active_operations or {}) == 0)

    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("title_menu token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("title_menu resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("title_menu token cancel adds no normal side effect", effects == beforeCancel)
    check("title_menu token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    local endings = coroutine.create(function() menu.show(ctx) end)
    check("title endings fixture opens", coroutine.resume(endings))
    for _ = 1, 3 do
        _G._GAME_KEY_DOWN = true
        check("title endings cursor advances", coroutine.resume(endings))
    end
    _G._GAME_KEY_ENTER = true
    check("title endings sub-screen suspends", coroutine.resume(endings)
        and coroutine.status(endings) == "suspended")
    check("title endings coroutine closes", coroutine.close(endings))
    check("title endings restores focus", ctx.input_focus == "history" and focus == "GAME")

    -- A retired Endings coroutine must not consume its replacement's input.
    ctx.seen_endings = {one = {name = "One", scene = "ending.ks"}}
    local retired = coroutine.create(function() menu.show(ctx) end)
    check("retired title fixture opens", coroutine.resume(retired))
    for _ = 1, 3 do
        _G._GAME_KEY_DOWN = true
        check("retired title cursor advances", coroutine.resume(retired))
    end
    _G._GAME_KEY_ENTER = true
    check("retired Endings sub-screen suspends", coroutine.resume(retired)
        and coroutine.status(retired) == "suspended")
    require("kag.operation").cancel_all(ctx)
    local replacement = coroutine.create(function() menu.show(ctx) end)
    check("replacement title opens", coroutine.resume(replacement))
    _G._GAME_KEY_ENTER = true
    local beforeResume = renders
    check("retired Endings coroutine exits", coroutine.resume(retired)
        and coroutine.status(retired) == "dead")
    check("retired Endings preserves replacement input", _G._GAME_KEY_ENTER == true)
    check("retired Endings does not jump", ctx._pendingJump == nil)
    check("retired Endings does not render", renders == beforeResume)
    check("retired Endings keeps replacement visible", ctx.input_focus == "title"
        and nodes._title_bg.visible == true and nodes._title_bg.texture ~= nil)
    coroutine.close(retired)
    coroutine.close(replacement)
    check("title_menu repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("title_menu hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("title_menu releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

-- chapter_select cancellation, normal exit and replacement ownership
do
    local cached = {}
    for _, name in ipairs({"chapter_select", "backend", "layers", "audio"}) do
        cached[name] = package.loaded[name]
    end
    local previousKAG = _G.KAG
    local inputKeys = {"_GAME_KEY_UP", "_GAME_KEY_DOWN", "_GAME_KEY_LEFT",
        "_GAME_KEY_RIGHT", "_GAME_KEY_ENTER", "_GAME_KEY_ESC", "_GAME_KEY_F",
        "_GAME_MOUSE_DOWN"}
    local previousInputs = {}
    for _, key in ipairs(inputKeys) do previousInputs[key] = _G[key] end
    local fixture <close> = setmetatable({}, {__close = function()
        for _, name in ipairs({"chapter_select", "backend", "layers", "audio"}) do package.loaded[name] = cached[name] end
        for _, key in ipairs(inputKeys) do _G[key] = previousInputs[key] end
        _G.KAG = previousKAG
    end})
    local nodes, textures, destroyed = {}, {}, {}
    local focus, effects = "GAME", 0
    package.loaded["backend"] = {
        get_resolution = function() return 1280, 720 end,
        get_input_focus = function() return focus end,
        set_input_focus = function(value) focus = value end,
        create_solid_texture = function()
            local id = #textures + 1; textures[id] = true; return id
        end,
        load_texture = function()
            local id = #textures + 1; textures[id] = true; return id, 640, 480
        end,
        destroy_texture = function(id) destroyed[id] = (destroyed[id] or 0) + 1 end,
        render_text = function() end,
        set_fullscreen = function() effects = effects + 1 end,
    }
    package.loaded["layers"] = {
        ensure = function(_, name)
            nodes[name] = nodes[name] or {}; return nodes[name]
        end,
        find = function(name) return nodes[name] end,
    }
    package.loaded["audio"] = setmetatable({}, {__index = function()
        return function() effects = effects + 1 end
    end})
    package.loaded["chapter_select"] = nil
    local menu = require("chapter_select")
    local ctx = {input_focus="history", f={}, sf={}, tf={}}
    ctx.labelMap = {chapter_one=1}
    local co = coroutine.create(function() return menu.show(ctx) end)
    local opened, err = coroutine.resume(co)
    check("chapter_select cancel fixture opens: " .. tostring(err), opened and coroutine.status(co) == "suspended")
    check("chapter_select exposes active focus", ctx.input_focus == "chapter")
    _G._GAME_KEY_ENTER = true
    check("chapter_select coroutine close succeeds", coroutine.close(co))
    check("chapter_select restores previous focus", ctx.input_focus == "history" and focus == "GAME")
    check("chapter_select clears pending menu input", _G._GAME_KEY_ENTER ~= true)
    check("chapter_select removes operation", #(ctx.active_operations or {}) == 0)

    for id in ipairs(textures) do
        check("chapter_select cancellation released texture " .. id, destroyed[id] == 1)
    end
    local normal = coroutine.create(function() return menu.show(ctx) end)
    check("chapter_select normal fixture opens", coroutine.resume(normal) and coroutine.status(normal) == "suspended")
    _G._GAME_KEY_ESC = true
    check("chapter_select normal close completes", coroutine.resume(normal) and coroutine.status(normal) == "dead")
    check("chapter_select normal close restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("chapter_select normal close removes operation", #(ctx.active_operations or {}) == 0)

    local tokenCancelled = coroutine.create(function() return menu.show(ctx) end)
    check("chapter_select token cancel fixture opens", coroutine.resume(tokenCancelled))
    local beforeCancel = effects
    require("kag.operation").cancel_all(ctx)
    check("chapter_select resume after token cancel exits", coroutine.resume(tokenCancelled)
        and coroutine.status(tokenCancelled) == "dead")
    check("chapter_select token cancel adds no normal side effect", effects == beforeCancel)
    check("chapter_select token cancel restores focus", ctx.input_focus == "history" and focus == "GAME")
    check("chapter_select repeated close succeeds", coroutine.close(co))
    require("kag.operation").cancel_all(ctx)
    for name, node in pairs(nodes) do
        check("chapter_select hides " .. name, node.visible == false and node.texture == nil)
    end
    for id in ipairs(textures) do
        check("chapter_select releases texture once " .. id, destroyed[id] == 1)
    end
    _G._GAME_KEY_ENTER = nil

end

print(string.format("RUNTIME MENU CLEANUP TESTS: %d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
