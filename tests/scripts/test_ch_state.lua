-- test_ch_state.lua — [ch] speaker state machine (audit)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

local KAG = require("kag")

-- pos whitelist: invalid -> center
local ctx = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    characters = {} }
pcall(KAG.ch, ctx, { name = "A", text = "hi", pos = "weird" })
check("invalid pos -> center", ctx.characters["A"].pos == "center")

-- position inheritance: second line without pos keeps stored pos
pcall(KAG.ch, ctx, { name = "A", text = "hi2", pos = "left" })
check("stored pos left", ctx.characters["A"].pos == "left")
pcall(KAG.ch, ctx, { name = "A", text = "hi3" })
check("pos inherited", ctx.characters["A"].pos == "left")

-- current_speaker tracked
check("current_speaker", ctx.current_speaker == "A")

-- sprite guard: empty sprite does NOT shadow storage/file
pcall(KAG.ch, ctx, { name = "B", text = "hi", storage = "hero.png" })
check("sprite from storage", ctx.characters["B"].sprite == "hero.png")
pcall(KAG.ch, ctx, { name = "B", text = "hi2", sprite = "" })
check("empty sprite keeps storage", ctx.characters["B"].sprite == "hero.png")

-- no-speaker line: no character registration
local ctx2 = { f = {}, tf = {}, sf = {}, mp = {}, variables = {},
    characters = {} }
pcall(KAG.ch, ctx2, { text = "narration" })
check("narrator no registration", ctx2.characters ~= nil
      and next(ctx2.characters) == nil)

if failed > 0 then os.exit(1) end
print("CH STATE TESTS DONE")
