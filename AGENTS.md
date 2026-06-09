# AGENTS.md — Caesura (AmeKAG)

## Engine Identity
**Caesura** — Cross-platform visual novel engine. Codename **AmeKAG**.

## Core Constraint
All Lua→C++ access MUST go through pure virtual interfaces (I*Backend) registered in BackendRegistry. No direct C++ class access from Lua.

## Key Files to Read First
- CONCEPTS.md — shared domain vocabulary (interfaces, patterns, libraries)
- ENGINE_ANALYSIS.md — module-by-module analysis with dependency graph
- README.md — quick start, features, architecture overview

## Knowledge Base
docs/solutions/ — documented solutions organized by category:
- rchitecture-patterns/ — design decisions and pipeline patterns
- uild-error/ — build system and CI fixes
- untime-errors/ — crashes and bug resolutions
Consult when implementing or debugging in documented areas.

## Build
`powershell
cmake -B build && cmake --build build --config Debug
`
Live2D is conditional: -DCAESURA_ENABLE_LIVE2D=ON (requires Cubism 5 Native SDK locally, never committed).

## Branch Convention
- codex/ prefix for feature branches
- Direct push to master allowed for solo development
- CI runs on cross-platform changes only