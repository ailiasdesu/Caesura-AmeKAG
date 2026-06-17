-- =============================================================================
--  Caesura (AmeKAG) �� kag.lua
--  KAG command handler table. The scheduler dispatches kag[cmd](ctx, params)
--  for every non-flow-control tag in the token stream.
--  Flow commands (if/jump/call/return/label/end/macro/eval/wait/stop)
--  are handled inline by scheduler.lua and never reach this module.
--
--  Phase 4: Loads all command sub-modules (layer, text, audio, system,
--  transition, video) and merges them into the unified KAG table.
-- =============================================================================

local KAG = {}
local flow = require("flow")

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Layer commands �� [bg], [fg], [cl], [image]
--  Loaded from kag/commands/layer.lua, wired to layers.lua + backend.lua.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local layer_cmds = require("kag.commands.layer")
for name, handler in pairs(layer_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Text commands �� [ch], [text], [l], [r], [er], [p]
--  Loaded from kag/commands/text.lua, delegates to backend font rendering.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local text_cmds = require("kag.commands.text")
for name, handler in pairs(text_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Audio commands �� [playbgm], [stopbgm], [playse], [playvoice], [fadebgm],
--  [xfadebgm], [stopse], [stopvoice], [waitsound], [waitbgm],
--  [setbgmvolume], [setsevolume], [setvoicevolume]
--  Loaded from kag/commands/audio.lua, wired to backend audio proxy.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local audio_cmds = require("kag.commands.audio")
for name, handler in pairs(audio_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  System commands �� [wait], [emb]
--  Loaded from kag/commands/system.lua. CancelToken-integrated blocking.
--  Note: [eval] is handled inline by scheduler.lua, not here.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local system_cmds = require("kag.commands.system")
for name, handler in pairs(system_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Saveplace / Loadplace �� in-memory scene bookmarks
--  Spec [10.2.38]: independent of save system, no disk writes.
--  Wired to system.lua System.saveplace/loadplace.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

do
    local System = require("system")
    KAG.saveplace = function(ctx, params) System.saveplace(ctx) end
    KAG.loadplace = function(ctx, params) System.loadplace(ctx) end
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Save/Load commands �� [save], [load], [listsaves]
--  Loaded from kag/commands/save.lua, wired to C++ SaveManager.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

do
    local save_cmds = require("kag.commands.save")
    KAG.save = save_cmds.save
    KAG.load = save_cmds.load
    KAG.listsaves = save_cmds.listsaves
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Transition commands �� [trans], [move], [quake], [fade]
--  Loaded from kag/commands/transition.lua, wired to GPU transition engine.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local trans_cmds = require("kag.commands.transition")
for name, handler in pairs(trans_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Video commands �� [video], [stopvideo]
--  Loaded from kag/commands/video.lua, wired to pl_mpeg backend.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local video_cmds = require("kag.commands.video")
for name, handler in pairs(video_cmds) do
    KAG[name] = handler
end

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  Resource commands �� [preload]
--  Spec [10.2.32]: async asset preloading with placeholder textures.
--  Loaded from kag/commands/resource.lua.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local resource_cmds = require("kag.commands.resource")
for name, handler in pairs(resource_cmds) do
    KAG[name] = handler
end


-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T
--  VFX commands �� [vfx type="particle|quake|shake|flash|fade|blur|stop"]
--  Loaded from kag/commands/vfx.lua, wired to vfx.lua + particle system.
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

local vfx_cmds = require("kag.commands.vfx")
for name, handler in pairs(vfx_cmds) do
    KAG[name] = handler
end
--  Legacy aliases �� backward compatibility
-- �T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T

-- [showtext] �� alias for [text]
KAG.showtext = KAG.text

-- [clearscreen] �� alias for [cl]
KAG.clearscreen = KAG.cl

-- [br] �� line break (decorative, same as [l])
function KAG.br(ctx, params)
    if KAG.l then KAG.l(ctx, params) end
end

-- [hr] �� horizontal rule (decorative, no-op)
function KAG.hr(ctx, params) end

-- [cancel] �� cancel current voice/transition (backward compat)
function KAG.cancel(ctx, params)
    local backend = require("backend")
    if backend.audio_stop then backend.audio_stop("voice") end
    ctx.waiting_input = false
end

-- [close] �� close active scene, return to menu (backward compat)
function KAG.close(ctx, params)
    local backend = require("backend")
    if backend.audio_stop then
        backend.audio_stop("bgm")
        backend.audio_stop("voice")
        backend.audio_stop("se")
    end
    -- Cancel all active operations
    if ctx.active_operations then
        for _, ct in ipairs(ctx.active_operations) do
            ct:mark_cancelled()
        end
        ctx.active_operations = {}
    end
    return "stop"
end

-- [macro] / [endmacro] / [erasemacro] �� handled inline by scheduler
-- These stubs exist for documentation only
function KAG.macro(ctx, params) end
function KAG.endmacro(ctx, params) end
function KAG.erasemacro(ctx, params) end

-- ===========================================================================
--  Lua → KAG flow-control API
--  Called from [iscript] blocks, [emb] expressions, or external Lua scripts.
--  These set ctx._next_index so the scheduler takes the jump on the next
--  coroutine resume.  Code after these calls still executes until the next
--  yield — use `return` to stop immediately after scheduling a jump.
-- ===========================================================================

--- kag.jump(ctx, target) — intra-scene label jump
function KAG.jump(ctx, target)
    if not ctx or not target then return end
    local idx = flow.find_label(ctx.tokens, target:gsub("^*", ""))
    if idx then
        ctx._next_index = idx
    else
        print("[kag.jump] Label not found: " .. tostring(target))
    end
end

--- kag.call(ctx, target) — subroutine call (push call stack, jump to label)
function KAG.call(ctx, target)
    if not ctx or not target then return end
    local idx = flow.find_label(ctx.tokens, target:gsub("^*", ""))
    if idx then
        ctx.call_stack = ctx.call_stack or {}
        table.insert(ctx.call_stack, {
            tokens = ctx.tokens,
            index  = ctx.token_index,
        })
        ctx._next_index = idx
    else
        print("[kag.call] Label not found: " .. tostring(target))
    end
end

--- kag.return_to_caller(ctx) — return from subroutine
function KAG.return_to_caller(ctx)
    if not ctx then return end
    if ctx.call_stack and #ctx.call_stack > 0 then
        local frame = table.remove(ctx.call_stack)
        ctx._next_index = (frame.index or 1) + 1
    end
end

-- Convenience wrappers for save/load from Lua
function KAG.save_game(ctx, slot, desc)
    if not ctx then return end
    local save_cmds = require("kag.commands.save")
    save_cmds.save(ctx, { slot = slot, desc = desc or "" })
end

function KAG.load_game(ctx, slot)
    if not ctx then return end
    local save_cmds = require("kag.commands.save")
    save_cmds.load(ctx, { slot = slot })
end

return KAG
