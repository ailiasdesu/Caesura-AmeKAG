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
    require("kag.operation").cancel_all(ctx)
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
    require("kag.operation").cancel_all(ctx)
    return "stop"
end

-- [macro] / [endmacro] / [erasemacro] �� handled inline by scheduler
-- These stubs exist for documentation only
function KAG.macro(ctx, params) end
function KAG.endmacro(ctx, params) end
function KAG.erasemacro(ctx, params) end

-- ===========================================================================
--  KAG 3.0 compatibility aliases
--  Lets scripts written for the Japanese KiriKiri/KAG3 tag set run mostly
--  unchanged on Caesura (differentiator vs Ren'Py/Tyrano).
-- ===========================================================================

-- [r] -- line break (KAG3); same as [l]
KAG.r = KAG.l or KAG.br

-- [s] -- KAG3 short-wait control char; unified through [wait]
-- (default 250ms matches KAG3's s-char pacing).
KAG.s = function(ctx, params)
    params = params or {}
    return require("kag.commands.system").wait(ctx, {
        ms = tonumber(params.ms) or 250,
    })
end
-- [delay ms=N] -- KAG3 duplicate of [wait]; unified through the wait
-- command + its schema contract (next-gen: one implementation, aliases
-- share it). delay's ms param maps onto wait's ms field.
KAG.delay = function(ctx, params)
    return require("kag.commands.system").wait(ctx, params)
end
-- [clear] -- clear text layer (KAG3); alias for [cl]
KAG.clear = KAG.cl

-- [ct] -- clear text/message (KAG3); same as [clear]
KAG.ct = KAG.cl

-- [waitforclick] -- block until the player clicks (KAG3 control flow)
function KAG.waitforclick(ctx, params)
    if not ctx then return end
    ctx.waiting_input = true
    while ctx.waiting_input do
        coroutine.yield()
    end
end

-- [ld] -- delete layer (KAG3 layer delete)
function KAG.ld(ctx, params)
    local layers = require("layers")
    local name = params.layer or params.name or params[1]
    if name then
        local node = layers.get_layer(name)
        if node then
            node.visible = false
            node.texture = nil
        end
    end
end

-- [shake] / [quake] -- screen shake (KAG3 classic effect)
function KAG.shake(ctx, params)
    local vfx = require("kag.commands.vfx")
    if vfx.shake then vfx.shake(ctx, params) end
end
KAG.quake = KAG.shake

-- [playstop] -- stop BGM (KAG3)
function KAG.playstop(ctx, params)
    local audio = require("kag.commands.audio")
    audio.stopbgm(ctx, params)
end

-- [voice file=X] -- play voice (KAG3)
function KAG.voice(ctx, params)
    local audio = require("kag.commands.audio")
    audio.playvoice(ctx, { file = params.file or params[1] })
end

-- [bgm file=X] -- KAG3 alternate; unified through [play bus=bgm]
KAG.bgm = KAG.play

-- [se file=X] -- KAG3 alternate; unified through [play bus=se]
function KAG.se(ctx, params)
    local audio = require("kag.commands.audio")
    audio.playse(ctx, { file = params.file or params[1] })
end

-- [play bus=bgm|se|voice file=X volume=...] -- next-gen unified audio
-- command: one entry for all three buses (KAG3 needed play/bgm/se/voice
-- as separate commands with duplicated param handling).
function KAG.play(ctx, params)
    local audio = require("kag.commands.audio")
    local bus = params.bus or "bgm"
    if bus == "bgm" then
        return audio.playbgm(ctx, { file = params.file or params[1], volume = params.volume })
    elseif bus == "se" then
        return audio.playse(ctx, { file = params.file or params[1], volume = params.volume })
    elseif bus == "voice" then
        return audio.playvoice(ctx, { file = params.file or params[1] })
    end
    print("[play] unknown bus: " .. tostring(bus))
end

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

-- NOTE: KAG.save_game / KAG.load_game are C bindings registered by
-- SaveBinding (src/script/bindings/SaveBinding.cpp). Do not redefine them
-- here: a Lua-level redefinition shadows the C functions and breaks the
-- [save]/[load] command path.

return KAG
