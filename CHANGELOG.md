# Changelog

All notable changes to **Caesura (AmeKAG)** visual novel engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-08-25 (GA Release)

### Added
- **KAG Unified Semantic Layer (`scripts/kag/semantic.lua`)**: Single source of truth for `.ks` AST, control flow graph, translatable text, and diagnostics.
- **Direct AST Compilation (`scripts/kag/compiler.lua`)**: Compiler directly consumes AST models without redundant re-parsing.
- **Story Flow Graph Generator (`scripts/generate_story_flow.py`)**: Generates Mermaid diagrams, JSON topologies, and `--lint` diagnostics for broken jumps and orphan labels.
- **i18n Localization Pipeline (`scripts/extract_i18n.py`, `scripts/import_i18n.py`, `scripts/lint_i18n.py`)**: AST-driven extraction with deterministic hash keys, CSV/PO exports, and runtime dictionary compilation.
- **Unified Creator CLI (`scripts/caesura.py`)**: `create`, `doctor`, `flow`, `i18n`, `check`, and `build` commands.
- **Web Editor Story Map (`web/story-map.mjs`)**: Visual story branching graph in Web player/editor.
- **Live2D Extended Bindings**: `[live2d_motion]`, `[live2d_expression]`, and `[live2d_lip_sync]` command contracts.
- **CI/CD GA Pipeline (`.github/workflows/release.yml`)**: Multi-platform automated builds with SHA-256 integrity verification.

### Changed
- Refactored `scripts/generate_story_flow.py` and `scripts/extract_i18n.py` to eliminate regex-based semantic drift.
- Preloaded `kag.semantic` into engine sandbox rules (`scripts/kag/init.lua`).
- Updated `demo/example_game/` showcase with full 6-scene multi-branch storyline and 100% zh/en/ja translation coverage.

### Fixed
- Fixed UTF-8 string decoding issues in Windows subprocess calls across creator scripts.
- Clamped Live2D motion/expression/lip-sync parameters to valid ranges with graceful null-backend fallbacks.

---

## [1.0.0-rc.1] - 2026-08-24

### Added
- Multi-platform First-VN parity verification across Windows, Linux, Web, and Android.
- Android JNI IME input bridge with Japanese Romaji/Kana/Kanji conversion.
- Web PWA offline caching with Service Worker (`web/sw.js`).
- 16-module architectural coupling enforcement with CI gating.
- Full 1,052 C++ doctest suite with 385,299 assertions.
- 158 Lua test suites and 319 Web Vitest unit tests.
