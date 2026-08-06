-- ===========================================================================
--  Caesura (AmeKAG) — kag/init.lua
--  KAG module entry point. Loads all core Lua libraries.
--  Spec: Modules are loaded in dependency order; tokenizer + scheduler
--  supersede the legacy parser.lua + conductor.lua.
-- ===========================================================================

-- Core script engine
local kag        = require("kag")
local tokenizer  = require("tokenizer")
local scheduler  = require("scheduler")
local kag_debug  = require("kag_debug")   -- KAG scene debugger (preload for sandbox)
local kag_runner = require("kag_runner")  -- KAG coroutine bridge (preload for sandbox)
local mods       = require("mods")        -- mod loader (preload for sandbox)
local replay     = require("replay")      -- input recording/playback (preload for sandbox)
local flow       = require("flow")

-- Graphics
local layers     = require("layers")
local rtt        = require("rtt")
local blend      = require("blend")
local transition = require("transition")
local transform  = require("transform")
local vfx        = require("vfx")

-- Audio
local audio      = require("audio")

-- System
local system     = require("system")
local config     = require("config")
local backend    = require("backend")

-- Save/Load (Phase 6)
local save_cmds  = require("kag.commands.save")

-- P1 extensions
local gallery    = require("gallery")
local music_room = require("music_room")
local pool       = require("pool")

local i18n       = require("i18n")
local settings   = require("settings")

-- Legacy (backward compat) — removed from auto-load
-- [R3-FIX] Legacy parser/conductor removed from auto-load. Use tokenizer + scheduler directly.
-- These modules remain available for explicit require() if old scripts need them.
-- local parser     = require("kag.parser")
-- local conductor  = require("kag.conductor")

-- Register global _T shortcut for i18n
_G._T = i18n.t

print("[kag/init] All KAG libraries loaded.")
