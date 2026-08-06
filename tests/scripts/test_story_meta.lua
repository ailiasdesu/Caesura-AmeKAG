-- test_story_meta.lua — [chapter]/[ending] story metadata (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- [ending] records with id/name/scene and same-id overwrite
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "scripts/demo_story.ks" }
pcall(KAG.ending, ctx, { id = "end01", name = "Good End" })
check("ending recorded", ctx.seen_endings and ctx.seen_endings.end01 ~= nil)
check("ending fields", ctx.seen_endings.end01.name == "Good End"
      and ctx.seen_endings.end01.scene == "scripts/demo_story.ks"
      and type(ctx.seen_endings.end01.at) == "number")
pcall(KAG.ending, ctx, { id = "end01", name = "True End" })
check("ending same-id overwrite", ctx.seen_endings.end01.name == "True End")
check("ending count stays 1", next(ctx.seen_endings) ~= nil
      and ctx.seen_endings.end02 == nil)

-- [ending] defaults (id="end", name="Ending end")
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.ending, ctx2, {})
check("ending defaults", ctx2.seen_endings["end"]
      and ctx2.seen_endings["end"].name == "Ending end")

-- [ending] exotic id keys (security: id is a plain table key -- no
-- metatable on seen_endings, no injection; keys survive as data)
local ctxX = { f = {}, tf = {}, sf = {}, mp = {}, variables = {} }
pcall(KAG.ending, ctxX, { id = "a..b", name = "X" })
pcall(KAG.ending, ctxX, { id = "id=1", name = "Y" })
pcall(KAG.ending, ctxX, { id = "0x5F", name = "Z" })
check("exotic ids stored as data", ctxX.seen_endings["a..b"] ~= nil
      and ctxX.seen_endings["id=1"] ~= nil and ctxX.seen_endings["0x5F"] ~= nil)
check("records intact", ctxX.seen_endings["a..b"].name == "X"
      and ctxX.seen_endings["id=1"].name == "Y"
      and ctxX.seen_endings["0x5F"].name == "Z")
check("no metatable injected", getmetatable(ctxX.seen_endings) == nil)

-- [ending] with a non-table seen_endings resets defensively
local ctx3 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    seen_endings = "corrupted" }
pcall(KAG.ending, ctx3, { id = "e1" })
check("ending type-guard reset", type(ctx3.seen_endings) == "table"
      and ctx3.seen_endings.e1 ~= nil)

-- [chapter] routes via _pendingJump (labelMap index) when a choice is made
-- (save + restore the module -- the mock PERSISTS in package.loaded
-- and poisoned the later test_chapter_select suite run -- audit)
local _cs_real = package.loaded["chapter_select"]
package.loaded["chapter_select"] = { show = function() return "chapter_2" end }
local ctx4 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "scripts/demo_story.ks",
    labelMap = { chapter_1 = 10, chapter_2 = 25 }, stop_flag = false }
local chosen = KAG.chapter(ctx4, {})
check("chapter returns chosen", chosen == "chapter_2")
check("chapter routes pendingJump", ctx4._pendingJump ~= nil
      and ctx4._pendingJump.index == 25
      and ctx4._pendingJump.target == "chapter_2")
check("chapter sets stop_flag", ctx4.stop_flag == true)

-- no choice -> no signal
package.loaded["chapter_select"] = { show = function() return nil end }
local ctx5 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    current_scene = "s.ks", labelMap = {}, stop_flag = false }
pcall(KAG.chapter, ctx5, {})
package.loaded["chapter_select"] = _cs_real
check("chapter no-choice no-op", ctx5.stop_flag == false
      and ctx5._pendingJump == nil)

if failed > 0 then os.exit(1) end
print("STORY META TESTS DONE")
