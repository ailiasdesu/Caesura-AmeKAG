-- test_chapter_select.lua — [chapter] selector contracts (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local ChapterSelect = require("chapter_select")

-- collect: chapter_* labels only, read marking via visited
local ctx = { current_scene = "s.ks",
    labelMap = { chapter_1 = 5, chapter_2 = 40, intro = 1, chapter3 = 60 },
    _visited_labels = { chapter_2 = true },
    seen_scenes = {} }
local chs = ChapterSelect.collect(ctx)
check("collect chapters only", #chs == 3)
local byName = {}
for _, ch in ipairs(chs) do byName[ch.name] = ch end
check("chapter_1 read state", byName["1"] and byName["1"].read == false)
check("chapter_2 visited read", byName["2"] and byName["2"].read == true)
check("chapter3 collected", byName["第3"] ~= nil)

-- no labelMap: empty (no crash)
check("no map empty", #ChapterSelect.collect({}) == 0)

-- show(): cancel (Esc) returns nil and routes nothing
local backend_backup = _G._CAESURA_BACKEND
_G._CAESURA_BACKEND = { render = function() return true end,
    platform = function(cmd)
        if cmd == "get_resolution" then return 1280, 720 end
        if cmd == "get_input_focus" then return "KAG" end
        return true
    end }
local layers_backup = package.loaded["layers"]
package.loaded["layers"] = { ensure = function() return { visible = true } end,
    set_layer_visible = function() end, set_z = function() end }
local ctx2 = { current_scene = "s.ks",
    labelMap = { chapter_1 = 5, chapter_2 = 40 },
    _visited_labels = {}, seen_scenes = {},
    f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
_G._GAME_KEY_UP, _G._GAME_KEY_DOWN, _G._GAME_KEY_ENTER = false, false, false
local co = coroutine.create(function() return ChapterSelect.show(ctx2) end)
coroutine.resume(co)
_G._GAME_KEY_ESC = true
local ok, chosen = coroutine.resume(co)
check("esc cancels", ok and chosen == nil)
package.loaded["layers"] = layers_backup
_G._CAESURA_BACKEND = backend_backup

-- SystemCommands.chapter routes the choice through _pendingJump
local System = require("kag.commands.system")
local real_show = ChapterSelect.show
ChapterSelect.show = function() return "chapter_2" end
local ctx3 = { current_scene = "s.ks", labelMap = { chapter_2 = 40 },
    f = {}, sf = {}, tf = {}, mp = {}, variables = {} }
System.chapter(ctx3, {})
ChapterSelect.show = real_show
check("chapter routes pending jump", ctx3._pendingJump
      and ctx3._pendingJump.target == "chapter_2"
      and ctx3._pendingJump.index == 40 and ctx3.stop_flag == true)

if failed > 0 then os.exit(1) end
print("CHAPTER TESTS DONE")
