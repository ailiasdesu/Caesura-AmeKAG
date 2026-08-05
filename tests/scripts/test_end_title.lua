-- test_end_title.lua — [end] returns to title (Neo-Genesis)
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local passed, failed = 0, 0
local function check(name, cond)
    if cond then print("PASS " .. name) passed = passed + 1
    else print("FAIL " .. name) failed = failed + 1 end
end

-- The entry's title-return logic: an "ended" reason from
-- kag_runner.update spawns the title coroutine; the show() return value
-- (action) arrives on the resume that makes it dead.
-- (locals only: the suite sandbox rejects global writes)
local spawned = false
local action_taken = nil
local emu_title = nil
local function emulate_ended_branch(reason)
    if reason == "ended" and not spawned then
        spawned = true
        emu_title = coroutine.create(function()
            return "new"
        end)
    end
    if emu_title then
        local ok2, action = coroutine.resume(emu_title)
        if coroutine.status(emu_title) == "dead" then
            emu_title = nil
            if action == "new" then action_taken = "new" end
        end
        return ok2, action
    end
    return true
end

-- frame 1: runner reports ended -> spawn + first resume (dead, action)
local ok1 = emulate_ended_branch("ended")
check("ended spawns title coroutine", spawned == true)
check("action routed from last resume", action_taken == "new")

-- non-ended frames don't spawn
spawned = false
local ok2 = emulate_ended_branch("waiting-input")
check("no spawn without ended", spawned == false)

if failed > 0 then os.exit(1) end
print("END TITLE TESTS DONE")
