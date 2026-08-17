# Caesura (AmeKAG) — Changelog

> This document is maintained alongside development. It is generated from
> Conventional Commits by `python scripts/gen_changelog.py` and then
> hand-polished into grouped, readable entries. Regenerate for the raw
> one-commit-per-line view; edit this file for the curated view.

Project version: **1.0.0** — see `CMakeLists.txt` / tag `v1.0.0` (from v1.0.0-alpha).

---

## v1.0.0 — Initial productized release (2026-08-17)

> The first stable line release, cut from `master` at round 112. Spans 30 rounds of
> hardening (81→112) on top of the `v1.0.0-alpha` baseline — the full stage-G
> productization campaign: real-GPU verification, a post-processing stack, a
> declarative layout/tween system, the bundled sample game, and one-click
> packaging/distribution. **1818 commits** since `v1.0.0-alpha` (raw per-commit
> listing: `CHANGELOG-v100.md`).

### Highlights

- **Example game 「The One-Way Reply / 《单程回信》」** (round 101/105/110/112) — a complete
  ~17.5 min, 3-ending (归零/同行/守约) bilingual zh/en visual novel under
  `demo/example_game/`, exercising the full KAG Neo-Genesis feature surface
  (branching, trust-differential state, dual save slots, i18n hot-switch, tween,
  SMA mini-game, ending gallery). End-to-end gate: `verify_sample_game.sh` 5/5 PASS.
- **Declarative UI primitives** — `[tween]` tween system (round 106) and the
  `[layout]` hbox/vbox/grid container family (round 107), both with desktop/Web
  parity and settings-migration pilot.
- **Post-processing stack v1** (round 102) — bloom, vignette, LUT color grade and
  soft-blur GPU passes (fxc-compiled DXBC inlined; GL/Metal/Vulkan identity-fallback),
  wired into the `[vfx]` command family and 6 new render interfaces.
- **Editor upgrades** — Scene Builder zero-code panel (round 108), Timeline/Debug/
  Inspector/VisualView depth, LSP rename & KAG3-param-alias diagnostics, settings
  panel, AiPanel; editor suite grew to >530 tests.
- **KAG3 ecosystem tooling** (round 111) — `.xp3` archive parser and TLG5/6 image
  decoders, plus a `kag3-migration.md` 6-step migration pipeline.
- **Packaging & distribution** (round 108/109/110) — `package_game.sh` one-click
  Web builds, `deploy-web.yml` GitHub Pages workflow, Web performance baselines, and
  the verified CPack Release ZIP (`CaesuraAmeKAG-1.0.0-Windows-AMD64.zip`, 87.9 MB /
  386 files).
- **Cross-platform hardening** (round 103) — Metal readiness, Android (NDK/arm64)
  build chain research, cross-platform verification matrix; CARC nonce-reuse
  detection and save-security audit (round 104).

### Platform quality at this release

- C++ doctest **976/976**, assertions 8858/8858 (Release-verified), 66 test files.
- Lua main suite **131/131** + **24 orphan** + Web **297** (20 files) + Editor **530**.
- Coupling budget PASS; 119 KAG contracts, 100% runtime coverage; 31 interfaces;
  three-platform CI green (Windows D+R / macOS / Linux / Package).

```
python scripts/gen_changelog.py --from-tag v1.0.0-alpha --tag v1.0.0   # raw regenerate
```

---

## Round 81 — Editor live cross-referencing & Job exception isolation

### Added
- **Editor**: live engine position cross-reference in the scene outline (G4-4).
- **Editor**: drive the live scene to a label via an *eval* jump (G4-3).
- **Script**: LSP label rename across definition and navigation sites.
- **Script**: i18n plural tables, numeric formatting, and CLI round-trip.
- **Tests (C++)**: job exception isolation + engine lifecycle round-trip.

### Fixed
- **Job**: isolate worker and main-thread callback exceptions.

### Docs
- Sync lua-modules i18n plural API and the editor LSP appendix.

---

## Round 80 — Scene outline panel & LSP label rename (80% milestone)

### Added
- **Editor**: scene outline parser and read-only panel wiring (G4 increment 2).
- **Script**: LSP label rename across definition + navigation sites.
- **Script**: i18n plural tables + numeric format + CLI round-trip.
- **Tests**: HTTP smoke over the round-71-80 command surface.
- **Tests (C++)**: job system boundary — completion, ordering, nesting, worker pool.
- **Tests (web)**: full tutorial regression sweep across 15 scenes.
- **Tests (C++)**: texture budget tiers/quota enforcement (G8).
- **Tests (C++)**: storage encryption round-trip — GCM/magic/tamper (G10).

---

## Round 79 — ks_check round-4 warnings & dead-code cleanup

### Added
- **Script**: `ks_check` round-4 warnings — missing expression, duplicate params,
  unreferenced labels, dead `[macro]`-`goto` paths.

### Changed
- **Script**: G12 dead-code cleanup (conductor/parser/debug_api/demo_minimal) + index regen.

### Tests
- **Tests (web)**: full tutorial regression sweep, 15 scenes.
- **Tests (C++)**: texture budget tiers/quota enforcement (G8); storage encryption GCM (G10).

---

## Round 78 — Scene outline parser & audio pre-init fix

### Added
- **Editor**: scene outline parser + read-only panel (G4 first increment).

### Fixed
- **Audio**: bus volumes now apply from pre-init values at `init`.

### Tests
- **Web**: i18n relocalize — page/choices/backlog hot-switch parity.
- **Script**: flow edge depth — nested switch, break, until, float-for, goto-into-loop.
- **Tests (C++)**: CARC archive tamper/keys/streaming boundary (G10); platform/input boundary (G11).

---

## Round 77 — Palette LUT binding, ks_check warnings & i18n parity

### Added
- **Web**: palette LUT binding, real i18n module, goto/i18n parity.
- **Script**: `ks_check` cross-scene / empty-target / dead-macro warnings.
- **Editor**: i18n completion + goto probes.

### Fixed
- **CI**: Lua 5.4 bare-name resolution + api-stats contract census regen (round-77 CI red).

### Tests
- **Tests (C++)**: resource provider-chain/async boundary (G10); audio bus boundary (G10).

---

## Round 76 — goto alias, i18n hot-switch & storage hardening

### Added
- **Script**: native `goto` jump alias + `ks_check` structural warnings wired.
- **Script**: `[i18n language=]` hot-switch command.

### Fixed
- **Script**: `[i18n]` handler params aligned with the contract.
- **CI**: platform-portable `ks_i18n` subprocess interpreter + editor i18n highlight (round-76 CI red).

### Tests
- **Script**: runtime contract depth — math/textspeed/preload/sel-x/macro;
  goto highlight + sel bridge probes (editor); player web parity.
- **Tests (C++)**: storage hardening — corrupt-save, slot-bounds, migration.

### Docs
- Sync api/design/guides with rounds 71-76 state.

---

## Round 75 — i18n runtime API & LSP completion

### Added
- **Script**: i18n runtime language API (`set_language` / `translate` / `reload`).
- **Script**: `ks_check`/LSP — sel alias param completion.
- **Script**: kag3_import macro args + `goto` alias.
- **Tests**: web player parity (loop/choice/nvl/ruby); LSP bridge + command highlight;
  headless RPC smoke over round-71-74 contracts; nested-brace interpolation regressions.

### Fixed
- **Script**: save/load loop-stack restore + macro depth guard + nested macro definitions.

---

## Round 74 — Choice capture, macro/system/save/LSP depth

### Added
- **Script**: choice result capture (`x=`), ruby cursor-follow, NVL layer restore.
- **Script**: `ks_i18n` CLI end-to-end + i18n guide.

### Docs
- **API**: regenerate command contracts (`button x` param).

### Tests
- **Script**: macro system depth + orphan-suite registration; save/load edge boundary;
  LSP definition/reference navigation depth; `ks_i18n` CLI e2e.

---

## Round 73 — Switch/loop/ternary depth, static analysis & kag3_import e2e

### Added
- **Script**: perfect LSP unknown-param diagnostics.
- **Script**: `ks_check` structural warnings — dangling jump/call/link targets,
  unused macros, duplicate switch cases.
- **Demo**: Tutorial 13 enables the `[palette]` demo (day/night/toggle).

### Fixed
- **Script**: kag3_import unquoted param aliases + end-to-end integration test.

### Tests
- **Script**: switch semantics depth; loop-range depth; perf-baseline extension;
  ternary @ index + assignment ternaries.

---

## Round 72 — KAG3-compat teaching & import closure

### Added
- **Script**: wire round-71 commands into the KAG3 import layer.
  New `PARAM_ALIASES` mechanism + `CONFLICT_NOTES` advisory for `[palette]`.
- **Demo**: Tutorial 13 — KAG3-compat commands in practice (textspeed/cps, csp/csd/csl, notify, vibrate, preload+bg).

### Fixed
- **Script**: `[palette]` runtime crash + `[preload]` contract enum (round-72 crash).

### Docs
- **API**: regenerate command contracts after the preload enum fix.
- Tutorial/sample-library + expression/eval + stale endpoint fixes.

---

## Round 71 — KAG3-compat command batch & LSP interpolation diagnostics

### Added
- **Script**: KAG3-compat command batch (7-agent fan-out): arithmetic (`add/sub/mul/div/mod/dec`),
  character (`csp/csd/csl`), text speed (`textspeed/cps`), effects (`palette/vibrate`), `notify`.
- **Script**: LSP interpolation diagnostics + completion/hover for `${}` expressions.

### Fixed
- **Script**: `[eval]` bare-value expressions + test-suite hardening.

### Docs
- **API**: regenerate command contracts (117) and census.

## Round 70 — Long-string scanning & CI census determinism

### Fixed
- **Script**: long-string-aware expression scanning (round-70 C2) — `[[...]]` literals
  no longer break ternary/`??`/`&&`/`||` translation.
- **Script**: `api_stats` locates the Lua interpreter under `build/` too.
- **CI**: make the docs census deterministic — drop environment-dependent live rows.
- **Build**: link `libm` for `lua_cli` on Linux.

### Tests
- **Script**: register `saveflow` + `dispatch-bench` in the orphan suite.

---

---

## Legend

- Conventional Commits types used: `feat`, `fix`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`, `plan`.
- Scopes: module (`script`, `editor`, `web`, `demo`, `job`, `audio`, ...) or layer (`p1`, `p2`, `backend`).
- Regenerate the raw listing: `python scripts/gen_changelog.py --from-tag v1.0.0-alpha` or `--count N`.

See `docs/guides/release-process.md` for the full release workflow.
