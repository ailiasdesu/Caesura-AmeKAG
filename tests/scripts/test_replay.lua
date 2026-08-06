-- test_replay.lua — input recording/playback: event capture with
-- coordinates, timing, JSON persistence, playback firing, kag_runner
-- integration (record on click; playback drives on_click).
package.path = "scripts/?.lua;scripts/kag/?.lua;" .. package.path
local results = {}
local function check(name, cond, detail)
    if cond then print("PASS " .. name) else print("FAIL " .. name .. " -- " .. tostring(detail)) end
    results[#results + 1] = cond
end

local replay = require("replay")

-- ---- record: events with timestamps + coordinates ------------------------
do
    replay.set_mode("record")
    replay.record("click", 100, 200)
    replay.record("click", 300, 400)
    check("record captures events",
        replay.event_count() == 2, tostring(replay.event_count()))
    replay.set_mode("off")
    check("off mode stops recording",
        replay.record("click", 1, 1) == false)
end

-- ---- playback: fire due events in order with coordinates -----------------
do
    replay.set_mode("record")
    replay.record("click", 10, 20)
    -- advance the clock between clicks (record mode ticks via update)
    replay.tick(500, nil)
    replay.record("click", 30, 40)
    local fired = {}
    replay.set_mode("playback")
    -- first click due at t=0: fires on the first tick
    replay.tick(0, function(x, y) fired[#fired + 1] = { x, y } end)
    check("first event fires on first tick",
        #fired == 1 and fired[1][1] == 10 and fired[1][2] == 20,
        tostring(#fired))
    -- second event due at t=500: not fired at t=16
    replay.tick(16, function(x, y) fired[#fired + 1] = { x, y } end)
    check("second event waits for its timestamp",
        #fired == 1, tostring(#fired))
    -- reaches t=500+ on the next ticks
    replay.tick(500, function(x, y) fired[#fired + 1] = { x, y } end)
    check("second event fires when due",
        #fired == 2 and fired[2][1] == 30 and fired[2][2] == 40,
        tostring(#fired))
    check("playback done after all events fired",
        replay.is_done() == true)
    replay.set_mode("off")
end

-- ---- JSON persistence round-trip ------------------------------------------
do
    replay.set_mode("record")
    replay.record("click", 111, 222)
    replay.record("click", 333, 444)
    local ok, err = replay.save("tests/scripts/_replay_tmp.json")
    check("save writes file", ok == true, tostring(err))
    replay.set_mode("off")
    local n, lerr = replay.load("tests/scripts/_replay_tmp.json")
    check("load reads events", n == 2, tostring(n) .. " " .. tostring(lerr))
    replay.set_mode("playback")
    local fired = {}
    replay.tick(0, function(x, y) fired[#fired + 1] = { x, y } end)
    replay.tick(16, function(x, y) fired[#fired + 1] = { x, y } end)
    check("round-trip preserves coordinates",
        fired[1][1] == 111 and fired[1][2] == 222
            and fired[2][1] == 333 and fired[2][2] == 444,
        tostring(fired[1] and fired[1][1]) .. "," .. tostring(fired[2] and fired[2][1]))
    replay.set_mode("off")
    os.remove("tests/scripts/_replay_tmp.json")
end

-- ---- kag_runner integration: on_click records -----------------------------
do
    local runner = require("kag_runner")
    -- A suite-chain ctx may exist with input_focus set (overlay guards);
    -- clear it so the record branch is reached. No coroutine runs here:
    -- the record branch fires, then on_click returns.
    local rctx = runner.get_ctx()
    if rctx then rctx.input_focus = nil end
    replay.set_mode("record")
    _G._GAME_MOUSE_X = 55
    _G._GAME_MOUSE_Y = 66
    runner.on_click()
    check("runner.on_click records mouse position",
        replay.event_count() == 1, tostring(replay.event_count()))
    replay.set_mode("off")
end

-- ---- [replay] command through the scheduler ------------------------------
do
    local tokenizer = require("tokenizer")
    local scheduler = require("scheduler")
    local kag_orig = package.loaded["kag"]
    local sys = require("kag.commands.system")
    package.loaded["kag"] = {
        replay = function(c2, p2) return sys.replay(c2, p2) end,
        ch = function() end,
    }
    local ctx = { f = {}, tf = {}, sf = {}, mp = {}, lf = {},
                  current_scene = "r.ks", token_index = 1, stop_flag = false }
    local co = coroutine.create(function()
        scheduler.run(ctx, tokenizer.parse([[
[replay mode="record"]
[eval exp="f.x = 1"]
]]), 1)
    end)
    while coroutine.status(co) ~= "dead" do coroutine.resume(co) end
    check("[replay mode=record] sets record mode",
        replay.get_mode() == "record", tostring(replay.get_mode()))
    local co2 = coroutine.create(function()
        scheduler.run(ctx, tokenizer.parse([[
[replay mode="off"]
]]), 1)
    end)
    while coroutine.status(co2) ~= "dead" do coroutine.resume(co2) end
    check("[replay mode=off] stops recording",
        replay.get_mode() == "off", tostring(replay.get_mode()))
    package.loaded["kag"] = kag_orig
end

local failed = 0
for _, ok in ipairs(results) do if not ok then failed = failed + 1 end end
if failed > 0 then os.exit(1) end
print("REPLAY TESTS DONE")
