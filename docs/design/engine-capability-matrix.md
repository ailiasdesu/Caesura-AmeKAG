# Engine Capability Matrix (Mermaid)

> 2026-08-15 readiness audit (refreshed to round 90 / 90% milestone): this matrix tracks 79 code-level capability surfaces (existing rows refreshed for round 88-90; new round-90 rows: contract runtime coverage, tutorial sample library).
> A present interface or conditional implementation is not counted as release validation.

## Readiness Snapshot

| Layer | Completion | Evidence boundary |
|---|---:|---|
| Modular architecture migration | 98% | Static/API targets, composition ownership, Engine-owned DebugProtocol, owner-thread RPC DTO dispatch, Lua-hook coexistence, shared production linkage and source-level gates are in place |
| Core visual-novel usability | 74% | Headless/KAG/save/audio unit paths strong; R7/R8/C2 verified on real GPU (D3D11 + OpenGL 4.3); mini-game Lua binding + demo end-to-end; remaining gap: Metal backend and macOS/Linux hardware validation |
| Cross-platform product release | 34% | Multi-platform CI builds exist; Windows D3D11+GL real-GPU pixels verified; optional SDKs (Steam/Live2D) and non-Windows packages not release-verified |

The percentages are deliberately conservative. Unit-test success demonstrates regression coverage,
not that optional SDKs, assets, drivers or editor transport have been exercised in production conditions.

```mermaid
graph LR
    subgraph "Rendering"
        r1["Multi-backend GPU<br/>D3D11 · OpenGL · Metal"]
        r2["Layer Compositing<br/>BG/FG/MSG · dirty rect"]
        r3["Texture Pipeline<br/>async load · budget · LRU"]
        r4["2D Particles<br/>emitter · physics · GPU"]
        r5["Video Playback<br/>MPEG-1 · FFmpeg"]
        r6["GPU Monitor<br/>adaptive quality · degrade"]
        r7["Text Rendering<br/>FreeType · CJK · Ruby"]
        r8["Transition Effects<br/>blend · wipe · custom"]
        r9["Render-to-Texture<br/>offscreen · viewport blit"]
        r10["Batch Protocol<br/>deferred GPU submit"]
    end

    subgraph "Scripting"
        s1["Lua 5.4 VM<br/>coroutine · sandbox"]
        s2["KAG Neo-Genesis Parser<br/>tokenizer · 117 commands"]
        s3["Flow Control<br/>if/jump/call/switch/macro"]
        s4["Instruction Budget<br/>anti-infinite-loop"]
        s5["Hot Reload<br/>script watch · live edit"]
        s6["Error Recovery<br/>pcall guard · ErrorUI"]
        s7["Conditional Wait<br/>[until exp timeout]"]
        s8["Conditional Choices<br/>[button cond]"]
    end

    subgraph "Audio"
        a1["3-Bus Audio<br/>BGM · Voice · SE"]
        a2["Cross-fade<br/>volume · position query"]
        a3["3D Spatial<br/>listener · position"]
        a4["Per-handle Volume<br/>SE control"]
    end

    subgraph "Content"
        c1["Live2D Animation<br/>Cubism 5 / PNG fallback"]
        c2["3D Mini-Games<br/>enter→render→leave"]
        c3["Save System<br/>JSON · AES-256-GCM"]
        c4["Schema Migration<br/>v1→v5 auto-upgrade"]
        c5["CARC Archive<br/>pack · encrypt · sign"]
        c6["Ed25519 Signing<br/>tamper detection"]
        c7["Cloud Saves<br/>provider abstraction"]
        c8["Steamworks<br/>achievements · stats"]
        c9["Asset Pipeline<br/>provider chain · priority"]
    end

    subgraph "Development"
        d1["Editor RPC<br/>HTTP + stdio DTO dispatch"]
        d2["Structured Logging<br/>ring buffer · subsystem stats"]
        d3["Frame Profiling<br/>GPU submits · Lua GC"]
        d4["NullJobSystem Mock<br/>synchronous testing"]
        d5["Headless Mode<br/>no-GPU test env"]
        d6["Dev Mode<br/>checkerboard placeholder"]
        d7["Lua Debugger<br/>yield pause · owner bridge"]
        d8["AI Dev Assistant<br/>explain · fix · review"]
    end

    subgraph "Platform"
        p1["Cross-Platform<br/>Win · Mac · Linux"]
        p2["CI Pipeline<br/>3-platform build+test"]
        p3["Thread Pool<br/>priority · onComplete"]
        p4["Input Routing<br/>KAG↔Game focus switch"]
        p5["Texture Budget<br/>auto-detect 6 tiers"]
        p6["Sandbox Quota<br/>resource limiting"]
    end
```

## Capability Breakdown

### Rendering (10 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| R1 | Multi-backend GPU (D3D11/OpenGL/Metal) | `IRenderDevice` | ✓ D3D11 + OpenGL 4.3 real-GPU verified; Metal engine-side complete (embedded shaders, backend selection, Live2D Metal render path implemented) -- runtime validation requires macOS hardware |
| R2 | 3-layer compositing (BG/FG/MSG) with dirty-rect optimisation | `ILayerManager` | ✓ |
| R3 | Async texture loading with budget enforcement + LRU eviction | `ITextureManager` | ✓ |
| R4 | 2D GPU particle system (emitters, physics, colour) | `IParticleSystem` | ✓ |
| R5 | Video playback (MPEG-1 via pl_mpeg, FFmpeg optional) | `IVideoPlayer` | ✓ |
| R6 | Adaptive GPU quality monitor with automatic degradation | `IGpuMonitor` | ✓ |
| R7 | Text rendering (FreeType atlas, CJK support, ruby/furigana) | `IRenderDevice` | ✓ CJK font (NotoSansCJKsc, OFL) + `text_set_font` face switching; D3D11 GPU smoke test loads real OTF (786 glyphs), renders CJK + ruby; `loadTTF` guarded against uninitialized GPU + glyph-failure diagnostics |
| R8 | Transition effects (blend, wipe, custom shader) | `IRenderDevice` | ✓ blend/wipe/custom-shader paths; Transition program compiled on D3D11 + OpenGL 4.3, demo flash+crossfade rendered on both backends (2026-08-06) |
| R9 | Render-to-texture with viewport blit | `IRenderDevice` | ✓ |
| R10 | Batch draw-call protocol for multi-layer scenes | `IRenderDevice` | ✓ |

### Scripting (38 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| S1 | Lua 5.4 VM with coroutine-based scheduler | `ILuaManager` | ✓ |
| S2 | KAG Neo-Genesis parser (118 contract commands, 9 categories) | Lua tokenizer | ✓ |
| S2a | KAG3 bare positional args (13 families, 15 commands: delay/wait/se/voice/play/jump/call/link/unlock/macro/erasemacro/save/load/gallery/ending) | tokenizer + scheduler | ✓ |
| S2d | KAG3 expression compatibility (TJS `&& \|\| ! != ?:` translated string-aware; visible scene:line errors instead of silent else) | `kag/expr.lua` | ✓ |
| S2e | KAG3 variable system (`%f.x%` interpolation, `lf` call-frame stack, `mp` message params, dual-style expression env) | schema + scheduler | ✓ |
| S2f | KAG3 control-flow completeness (`[elsif]` alias, `[call *label]` intra-scene, `[end]`→ending, unknown-tag warnings) | tokenizer + scheduler | ✓ |
| S2g | Modern utility commands (`[set]` typed, `[inc]`, `[random]`, `[assert]`) | system commands | ✓ |
| S2h | Command metadata (category/blocking/desc on all 118 contracts; emitted by schema_doc + dumpContracts) | schema | ✓ |
| S2b | Exact token offsets (byte-accurate '[' position + end_offset via dual Cp) | `parse_with_offsets` | ✓ |
| S2c | Parser performance (4000 tokens 362ms -> 259ms, 64.75ms/1000tok after dropping 9 redundant prefix patterns) | lpeg.lua | ✓ |
| S2i | KAG scene-level debugger (breakpoints on scene+cmd/line, single-step, scope inspection; RPC kagSetBreakpoint/kagDebugContinue/kagDebugStep/kagInspectScopes) | `kag_debug.lua` + scheduler + RPC | ✓ |
| S2j | Mod loader (scene/resource override by priority; mods/<name>/<path> wins over base; path-traversal guarded) | `mods.lua` + flow + resolve_file | ✓ |
| S2k | Input recording/playback (auto-demo + regression; [replay] command, JSON persistence, choice coordinates) | `replay.lua` + kag_runner | ✓ |
| S2l | Accessibility (closed captions for voiced lines, settings toggle, TTS interface probe) | text commands + settings | ✓ |
| S2m | Scene hot reload (edit a running .ks -> re-parse + position remap + state preserved; HotReload watches assets/script + mods; RPC kagReloadScene) | `kag_runner.reload_scene` + HotReload + flow | ✓ |
| S2n | LLM-driven dialogue ([ai_dialog] async query; OpenAI-compatible / Ollama endpoints; graceful fallback text; AI binding + sandbox whitelist) | `AIBinding` + system commands + backend | ✓ (mock + **real Ollama e2e verified** on 2026-08-13: engine --headless RPC eval against live Ollama gemma3:4b — sync AI.query + async AI.query_async both return real replies; **model auto-discovery**: empty config model on Ollama endpoints asks the server (GET /api/tags, cached) instead of 404-ing on a hardcoded default; read timeout 15s→60s for cold loads; conditional C++ test (skips when Ollama unreachable) + ctest CaesuraHeadlessAiSmoke (exit 77 skip on CI)) |
| S2o | Demo/video export (recorded replay drives the game while each frame is captured to PNG; --export-replay/--export-dir; screenshot readback fixed) | `Engine::run` + `BgfxDebugCallback::screenShot` | ✓ |
| S2p | Colorblind/high-contrast filter (config.accessibility.color_filter presets; VFX effect 4 matrix pass over RTT scene layers; D3D11 + GL shaders) | `IRenderDevice::setColorFilter` + fs_vfx effect 4 | ✓ (RTT scene layers; direct-drawn UI text unaffected) |
| S2q | Declarative conditional wait (`[until exp="..." timeout=ms]` — wait until a TJS expression is truthy; per-frame yield + cancellation; Ren'Py needs python loops for this) | scheduler + compiler (AOT exp/dump) | ✓ |
| S2r | Conditional choices (`[button cond="f.x > 1"]` — false choices hidden at [endbutton]; Ren'Py menu `if` parity; all-hidden blocks dissolve) | `kag/commands/text.lua` + `kag/expr.lua` | ✓ |
| S2s | Inline text markup (`{color=#rrggbb}` per-span colors, `{size=N}` glyph scaling affecting wrap, `{b}` synthetic bold, `{i}` italic top-edge shear, `{s}` strikethrough middle bar — all stackable; unknown `{tags}` literal) | `kag/text_layout.lua` (parse_markup/wrap_spans) + `kag/text_scene.lua` (add_wrapped_spans) + `IRenderDevice::renderText(scale, bold, italic, strike)` | ✓ (34 Lua assertions; typewriter reveal + backlog use stripped plain text; strike bar geometry headless-tested) |
| S2t | NVL mode (`[nvl]` full-screen accumulated text block, Ren'Py parity; `[nvl clear]`/`[p]` page break, `[nvl off]` exit; speaker as 「Name」： inline prefix styled by `nameplate_style.text_color` — format customizable via `[nvl prefix="%s："]` schema param (persists per session, `%`-safe); wraps with the line, instant draw — zero new state fields; cursor reused from text_state so save/rollback persists the page) | `kag/commands/text.lua` (`nvl` handler + ch/text/p/er accumulation) + `kag/text_scene.lua` (`commit` seals prior reveal draws, `instant` draws skip typewriter) + save/snapshot `nvl_mode` | ✓ (29 Lua assertions; typewriter only animates the appended line) |
| S2u | Localization pipeline (`{key}` token expansion with markup-name whitelist + per-line translation `lines["<scene>:<fnv1a(msg)>"]`, content-addressed keys; applied by [ch]/[text] before markup parsing **and by [button]/[sel] choice labels at registration**; empty placeholder falls back to original; settings Language hot-switch **+ full-page redraw** (already-displayed page / NVL page / backlog / active choice labels / closed captions re-localize with the new language via `page_src` replay — exceeds Ren'Py, which keeps displayed lines in the original language; layout y-cascade for re-wrapped translations; typewriter sealed) + save persistence (`state.language` + backlog `src`); `scripts/ks_i18n.lua` template generator with --update merge and --missing CI gate, extracts dialogue + choice labels) | `scripts/i18n.lua` (fnv1a/localize/expand/load) + `kag/commands/text.lua` (ch/text/button localize + relocalize_page/relocalize_backlog) + `kag/text_scene.lua` (page_src lifecycle) + `assets/lang/{zh,en,ja}.lua` | ✓ (67 Lua assertions; generated ja.lua template with 25 demo keys incl. choice labels) |
| S2v | Contract runtime coverage (**118/118 contracts execute at runtime**; round-90 grep audit: 106 had runtime refs, 12 previously-dead handlers — cancel/setvoicevolume/setsevolume/playbgmstop/playstop/waitforclick/moveto/camera/sprite_fade/move/scale/swap — now exercised; coverage matrix saturated) | scheduler + `test_contract_runtime_gaps.lua` (orphan 18→19) | ✓ (25 Lua assertions: headless no-crash + skip/fallthrough presence + key semantics) |
| S3 | Flow control (if/else, jump/call/return, switch/case, macros) | Lua scheduler | ✓ |
| S3a | Save/load loop continuity (for/while/if/switch stacks lifted into ctx and serialized via `loop_stacks` in capture_state; [load] resumes an in-progress loop to completion) | Lua scheduler + save snapshot | ✓ |
| S7 | Declarative command contracts (typed params, clamping, $var/${expr} interpolation, required/choices) | `kag/schema.lua` | ✓ |
| S8 | Static .ks validator + contract audit gate (ks_check --audit-defaults, CI) | `scripts/ks_check.lua` | ✓ |
| S8a | Truncation detection (offset stream stops before end-of-input + trailing comment handling) | ks_check | ✓ |
| S9 | Parameterized macros (args + %arg% substitution, nested expansion, deep-copied splice) | Lua scheduler | ✓ / nested macro **DEFINITIONS** ([macro outer][macro inner]...[endmacro][endmacro]) + depth-based recursion guard (>100 splice depth errors; 1000+ sequential calls & 2000-iteration loops pass) |
| S10 | Label index (O(1) jump, scene-scoped, restored/invalidated on swap) | Lua scheduler | ✓ |
| S11 | [if] expr cache keyed by env identity (no stale variables across scenes) | Lua scheduler | ✓ |
| S12 | i18n runtime API (set_language/current_language/translate/reload/default_language; mid-scene language switch, fallback chain, hot-reload) | `scripts/i18n.lua` + text pipeline relocalize_page | ✓ |
| S4 | Instruction budget sandbox (anti-infinite-loop, per-frame cap) | `ILuaManager` | ✓ (preserved through DebugProtocol attach/detach, breakpoint yield/resume and inherited coroutine hooks) |
| S5 | Hot reload (watch scripts/, live-reload without restart) | Engine-owned `HotReload` instance | ✓ |
| S6 | Error recovery (pcall guards, ErrorUI, graceful degradation) | scheduler + bindings | ✓ |
| S6a | Modal UI input loops (settings/music_room/gallery/chapter select -- per-frame yield + GAME key routing: UP/DOWN/LEFT/RIGHT/ENTER/ESC/F/mouse) | ui modules + Engine input | ✓ |
| S6b | UI state persistence (settings values, favorites, unlock sets, visual text state) | settings/music_room/save | ✓ |

### Audio (4 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| A1 | 3-bus audio (BGM with cross-fade, Voice with interrupt, SE) | `IAudioBackend` | ✓ |
| A2 | Fade, position query, per-bus volume control | `IAudioBackend` | ✓ |
| A3 | 3D spatial audio (listener position, 3D SE placement) | `IAudioBackend` | ✓ |
| A4 | Per-SE-handle volume and stop control | `IAudioBackend` | ✓ |

### Content Systems (10 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| C1 | Live2D animation (Cubism 5 SDK / PNG static fallback) | `IAnimationBackend` | Partial: PNG fallback + D3D11 (Windows) verified; **Metal render path fully implemented (was stub); GL shader deployment fixed (active-renderer FrameworkShaders copy)** -- GL/Metal runtime validation needs Linux/macOS hardware. SDK is bundled in thirdparty/. See `docs/guides/live2d-setup.md` |
| C2 | 3D mini-game framework (enter→update→render→leave loop) | `IMiniGameBackend` | ✓ lifecycle + JSON scenes + 20-API Lua binding (`mini_game` global, sandbox-whitelisted); real-GPU D3D11 child-process test (enter→update→render→leave) + programmatic `enter(0)` mode; demo_minigame.lua runs end-to-end on D3D11 and OpenGL 4.3 |
| C3 | Encrypted save/load (JSON, AES-256-GCM) | `ISaveManager` | ✓ round-trip suite (round 79): ciphertext unreadable without key, graceful no-key failure, wrong-key GCM rejection, magic-gated plaintext pass-through, forged-magic rejection, tampered ciphertext+nonce rejection, slot-boundary mirroring |
| C4 | Schema migration (v1→v5 auto-upgrade, pluggable migrations) | `ISaveManager` | ✓ |
| C5 | CARC archive packaging (compress, encrypt, sign) | `IArchiveWriter` | ✓ |
| C6 | Ed25519 digital signature (tamper detection for .carc files) | `ICryptoEngine` | ✓ |
| C7 | Cloud save provider abstraction (local / remote pluggable) | `ISaveProvider` | ✓ abstraction + local path + **HTTP cloud provider (push/pull/delete against a REST endpoint, offline degrade) + Lua save.configure_cloud/cloud_push/cloud_pull; mock-server round trip tested** |
| C8 | Steamworks integration (achievements, stats, cloud saves) | `ISteamBackend` | Conditional (needs Steam SDK/account): full Lua surface (19 APIs incl. cloud list/write/read/delete/quota), overlay/stats/store fixes, Null-backend tested; real SDK round trip needs a Steam dev account |
| C9 | Asset provider chain (Dir → CARC, priority-ordered, integrity check) | `IAssetProvider` | ✓ |
| C10 | Tutorial sample library (**15 递进式教程** tutorial_01–15, each runnable to [end] with line-by-line commentary; covers hello/text/layers/audio/branching/effects/saveload/system-ui/interpolation/loops/switch/expr-combo/commands/flow-timing/expr-deep; engine compile + Web-player double verification) | `demo/tutorial/*.ks` + `assets/lang/{zh,en,ja}.lua` | ✓ (round 90 added 14/15; ks_check zero WARN; sample-library path table 15 rows) |

### Development Tools (10 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| D1 | Editor RPC (HTTP plus stdio JSON-RPC) | `IEditorServer`, `IRpcServer`, `IRpcDispatcher` | Full: both transports use owner-thread DTO dispatch and are CLI-wired; managed-coroutine `run/eval` + breakpoint lifecycle (set/remove/clear/continue) + inspect + frame capture implemented on both transports; stdio smoke (`headless_rpc_smoke.py` 45/45) and HTTP smoke (`headless_http_smoke.py`, 46 assertions) end-to-end tested via ctest; **web full-tutorial regression sweep** (15 parametrized scenarios: tutorial 01–13 + showcase + example_game, all DONE; web gaps 62→79) + **G4-2 SceneOutline panel** (active-document outline rendering, label-click reveal navigation; editor 205→210) + **G4-3 outline-driven live jump** (label-click drives the running scene to a `*label` via `/api/eval` + `flow.find_label` + `kag.jump` `_next_index` — zero engine change; editor 210→234) + **G4-4 live engine position cross-reference** (`useEnginePosition` hook polls `/api/eval` for `_CAESURA_CTX`; outline row highlight + panel ▶ scene name; editor 234→245) + **InspectorView depth** (live engine status bar + `commandLint.ts` param lint with `KNOWN_COMMANDS` single source; label bidirectional jump + follow engine position; editor 245→270) + **editor store depth** (round 88: store actions + reveal-queue consumption, editor 333→368) + **rpc client depth** (round 89: evalRaw POST / response parsing / error propagation / URL encoding, editor 368→402) + **panel integration E2E** (round 90: `panels.integration.test.tsx` mounts real App+store — doc→outline→reveal→Inspector chain, setEngine broadcasts across StatusBar/Outline/Timeline/VisualView, ActivityBar mount/unmount; editor 402→408) |
| D2 | Structured logging (ring buffer, subsystem error counts, per-subsystem stats) | `IDebugManager` | ✓ |
| D3 | Frame profiling (GPU submit count, transient allocs, Lua GC timing) | `IDebugManager` | ✓ |
| D4 | NullJobSystem mock (synchronous task execution for deterministic testing) | `IJobSystem` | ✓ |
| D5 | Headless mode (no-GPU Engine init for CI/test environments) | `EngineConfig` | ✓ |
| D6 | Dev mode (checkerboard placeholder textures, verbose logging) | `ITextureManager` | ✓ |
| D7 | Lua debugger (breakpoints, step control, inspection) | `DebugProtocol` | ✓ Engine lifecycle, KAG resume arbitration, stdio commands, stale pause rejection and managed result/error cleanup are tested |
| D8 | AI dev assistant (`kag/aidev.lua`: local rule-based diagnostic explainer + structural scene review [flow balance / missing [end]]; LLM-enriched explanations, fix suggestions, full scene generation with self-review; exposed to the IDE via /api/eval) | `kag/aidev.lua` + `backend.ai_query` + AiPanel Dev Assist section | ✓ (26 Lua assertions; local paths work offline, LLM degrades gracefully) |
| D9 | LSP navigation (goto-definition / find-all-references for `*label` ↔ `[jump]`/`[call]`/`[link]`; cross-scene targets return name-only) | `kag/lsp.lua` (definition/references) + Monaco providers | ✓ (12 Lua assertions; Ctrl+Click + context menu in IDE) |
| D10 | Skeletal mesh animation (SMA S1–S5: CPU soft-skinning math + **GPU compute skinning** (bgfx compute; D3D11 DXBC + GL 430 GLSL, draw transform baked into bone-buffer slots, final NDC output — verified pixel-identical to the CPU path on a real D3D11 device) + bgfx renderer + Lua driver [JSON/hierarchy/LERP] + **round-18 playback controls** (loop/duration/rate/pause/resume/seek/play_anim crossfade/on_done_anim) + **2-bone IK** + **E-mote parts/variant switching** + `[sma_play]`/`[sma_anim]`/`[sma_ik]`/`[sma_variant]`/`[sma_stop]` contracts + `sma.*` binding incl. `set_skin_mode`) | `IMeshRenderer` + `SmaMeshRenderer` + `SmaSkinner` + `kag/sma.lua` + `SmaBinding` | ✓ (12 C++ + 70 Lua assertions; **D3D11 GPU child test: GPU skin == CPU skin pixel-for-pixel (14 assertions)**; headless uses Null backend; Metal/SPIR-V fall back to the CPU skinner) |

### Platform Infrastructure (7 capabilities)

| # | Capability | Interface | Status |
|---|-----------|-----------|--------|
| P1 | Cross-platform (Windows MSVC, Linux GCC, macOS Clang) | `IPlatformBackend` | Partial: CI build coverage; real GPU behavior is not verified on all platforms |
| P2 | CI pipeline (3-platform build + doctest suite, GitHub Actions) | `.github/workflows/ci.yml` | ✓ (round 90 baseline: C++ 849/849, Lua 126/126 + 19 orphan, web 183/183, editor 408/408, ctest 10 + AI-skip, coupling/coverage PASS; Release build + CPack ZIP 87.9MB measured end-to-end) |
| P3 | Multi-threaded task system (priority queues, main-thread callbacks) | `IJobSystem` | ✓ |
| P4 | Input routing (KAG ↔ Game focus switch, resize callbacks) | `IInputRouter` | ✓ |
| P5 | Texture budget auto-detection (6 tiers, 128MB–4GB) | `ITextureBudget` | ✓ enforcement tests (round 79): tier-boundary exact mapping, tier5 override-only, quota-full reject + release recovery, quota-0 all-reject |
| P6 | Lua sandbox resource quotas (textures, emitters, handles) | `ISandboxQuota` | ✓ |
| P7 | MobileAdapter (lifecycle callbacks, touch → mouse/wheel event mapping, DPI scaling) | `IMobileAdapter` (platform) | ✓ core mapping + lifecycle + **SDL finger events bridged (normalized -> pixel) + orientation change events (Lua _G.onOrientationChanged); 87 unit tests**; native mobile SDK integration still needs a device |

---

**Total: 79 tracked capabilities across 6 domains (incl. round-90 additions S2v contract coverage + C10 tutorial library; P2/D1 baselines refreshed).** See the readiness snapshot above for
the distinction between architecture completion, core usability and release readiness.

### 2026-08-12 additions (generation-gap round 9)

- S2t — NVL mode `[nvl]`/`[nvl clear]`/`[nvl off]`: full-screen accumulated
  text (Ren'Py NVL parity) with page-break cursor reuse and save/rollback
  persistence of the page.
- S2 — command set census refreshed to 81 contracts (nvl + sma_play/sma_stop).

### 2026-08-15 additions (round 71-75)

- S3a — save/load loop continuity: for/while/if/switch stacks
  lifted into the engine ctx and serialized via `loop_stacks` in
  capture_state; [load] resumes an in-progress loop to completion.
- S9 — nested macro definitions ([macro outer][macro inner]...[endmacro][endmacro])
  plus a depth-based recursion guard (>100 splice depth errors;
  1000+ sequential calls and 2000-iteration loops pass).
- S12 — i18n runtime API (set_language/current_language/translate/reload/default_language; fallback chain; hot-reload).
- [sel]/[button] `x=` result-capture.
- kag3_import macro-arg (`&N`/`%N%` conversion) + `goto`→`jump` alias.
- S2 — command set census refreshed to 117 contracts (add/sub/mul/div/mod/dec,
  csp/csd/csl, textspeed/cps, palette/vibrate, notify, preload, goto alias).
  (118 after round-86 `_meta` completion: 11 KAG3-compat commands gained
  {category,blocking,desc} metadata.)

### 2026-08-15 additions (round 88-90 / 90% milestone)

- S2v — contract runtime coverage 100%: 118/118 commands execute at runtime;
  12 previously dead handlers exercised by `test_contract_runtime_gaps.lua`.
- C10 — tutorial sample library grown to 15 tutorials (round 90 added
  tutorial_14_flow_timing + tutorial_15_expr_deep), path table +2 rows.
- P2 — round 90 test baseline (C++ 849/849, Lua 126+19, web 183, editor 408)
  plus Release build + CPack ZIP verified end-to-end.
- D1 — editor depth: store actions (round 88) + rpc client (round 89) +
  full-App panel-integration E2E (round 90), editor 270→408.

### 2026-08-12 additions (generation-gap round 6)

- S2s — inline text markup `{color=#rrggbb}`…`{/color}` (Ren'Py `{...}` parity;
  b/i/size consumed for source compat; unknown tags literal).
- D9 — LSP navigation: goto-definition / find-all-references for labels.
- D10 — Skeletal mesh animation S2/S3 (CPU soft-skinning + bgfx renderer + Lua
  driver + commands + binding).

### 2026-08-12 additions (generation-gap round 5)

- S2q `[until exp=... timeout=...]` — declarative conditional wait (AOT-compiled
  expression, per-frame yield, Operation cancellation, timeout guard).
- S2r `[button cond=...]` — conditional choices at [endbutton] (Ren'Py menu `if`
  parity; TJS conditions evaluated per block; all-hidden blocks dissolve).
- D8 `kag/aidev.lua` — AI-assisted development: local rule-based diagnostic
  explainer, structural scene review, LLM fix suggestions + scene generation
  with self-review; IDE surface via /api/eval (AiPanel Dev Assist section).

### Performance notes (2026-08-07 pass)

- Tokenizer: 4000-token scene parses in ~205 ms (52 ms/1000tok) after the
  -28.5% prefix-pattern pass; scene cache avoids re-parsing.
- Scheduler: 4001 coroutine resumes in ~16 ms (-36%): schema require
  hoisted out of the per-token loop; schema.coerce builds its location
  string lazily (error paths only).
- ks_check.lineOf: line-start index + binary search replaces per-token
  O(n) text scanning (O(n²) -> O(n log n)) — editor keystroke latency.
- expr.translate: bounded compiled-chunk cache (per-env identity).

### Module hardening pass (2026-08-07, per-module sweep)

- audio: 4-slot voice pool (overlapping lines fade, not cut) + BGM
  ducking (35% while a voice plays, exact restore); wave-cache LRU
  UB fixed (flushWaveCache rebuilt the LRU index; eviction guards)
- steam: overlay state via GameOverlayActivated_t (was IsOverlayEnabled
  -> permanent input pause); stats loaded via RequestCurrentStats +
  UserStatsReceived_t; StoreStats batched (1s throttle)
- input/entry: SDL_EVENT_WINDOW_RESIZED routed through
  InputRouter::notifyResize (was documented but never wired); per-frame
  GPU-time globals only written on change; single instruction-budget
  reset per frame
- archive: thread-local BCrypt key-handle cache (algorithm + key object
  reused across blocks; move-semantic RAII); save JSON compact (-40%)
- debug/job: LogEntry fixed char[256] (zero alloc per log call);
  isWorkerThread O(1) thread_local (was O(n) scan); dead member removed
- script/debug: RenderBinding getters cache backend pointers (registry
  lookup per binding call removed); DebugProtocol line hook fast path
  when no breakpoints/step
- render: fillViewport solid-pixel texture cached per color (was a GPU
  texture create per call); dead SimBatch worker + shouldUsePlmpeg
  removed
- minigame: sweep-and-prune collision detection (O(n^2) -> O(n log n))
- resource: decoded-asset cache (instant scene re-entry, cancelAll
  clears); ImageDecoder stb-first order fixed a 1x1-PNG crash in
  bimg::imageParse; dead cancel-generation field removed
