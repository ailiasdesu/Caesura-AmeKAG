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

-- P1 extensions
local gallery    = require("gallery")
local music_room = require("music_room")
local i18n       = require("i18n")
local settings   = require("settings")

-- Legacy (backward compat)
local parser     = require("kag.parser")
local conductor  = require("kag.conductor")

print("[kag/init] All KAG libraries loaded.")