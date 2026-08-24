-- =============================================================================
--  demo/sma_entry.lua — SMA showcase entry point (round 19).
--  Independent from the main galgame demo: spawns the hero actor with a
--  runtime-injected solid texture, then plays demo/sma_demo.ks which
--  drives animation switching, part variants and IK via KAG commands.
--  Usage: point the engine at this file (demo mode) and run.
-- =============================================================================
local kag_runner = require("kag_runner")
local layers = require("layers")
local sma_driver = require("demo.sma_demo_driver")

-- Pre-register the hero asset and spawn it BEFORE the script runs, so
-- [sma_anim]/[sma_variant]/[sma_ik]/[sma_stop] find the actor. The tex
-- parameter of [sma_play] takes a literal id; the entry creates the
-- texture at runtime instead (repo ships no PNG assets).
local function prepare()
    local asset = sma_driver.load_asset()
    if not asset then
        print("[SMA Entry] FATAL: cannot load " .. sma_driver.asset_path)
        return false
    end
    return true
end

local started = prepare()
if not started then return end

-- Start the KAG script; the actor is spawned lazily by the driver the
-- first time the script references it via [sma_anim] (commands are inert
-- without an actor, so spawn explicitly right after start).
local ok_start = kag_runner.start("demo/sma_demo.ks")
if not ok_start then
    print("[SMA Entry] FATAL: failed to start demo/sma_demo.ks")
    return
end

-- Spawn the actor in the live ctx once the runner has initialized it.
local spawn_done = false

function engine_update(dt)
    if not spawn_done then
        local ctx = _G._CAESURA_CTX
        if ctx and ctx.sma_actors == nil then
            -- wait until the runner ctx exists; spawn once
        end
        if ctx then
            sma_driver.spawn_hero(ctx)
            spawn_done = true
        end
    end
    kag_runner.update(dt or 0.016)
    local ctxS = _G._CAESURA_CTX
    if ctxS and ctxS.sma_actors then
        pcall(function()
            require("kag.sma").update(ctxS, dt or 0.016)
        end)
    end
end

function engine_render()
    layers.render()
    kag_runner.render()
    local ctxS = _G._CAESURA_CTX
    if ctxS and ctxS.sma_actors then
        pcall(function()
            require("kag.sma").render(ctxS)
        end)
    end
end

function _KAG_onClick()
    kag_runner.on_click()
end

print("[SMA Entry] loaded — demo/sma_demo.ks")
