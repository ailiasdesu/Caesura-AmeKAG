# Caesura (AmeKAG) — Release Process

This guide walks through a Windows desktop release: building, running the
release gates, generating the changelog, packaging with CPack, verifying the
ZIP, and publishing a GitHub release. The workflow mirrors the CI `release`
job in `.github/workflows/ci.yml` (CPack ZIP, Windows only).

## 0. Prerequisites

- **Git**: with `git` on `PATH`.
- **CMake 3.25+** and a Visual Studio 2022 toolchain.
- **Python 3.8+** (stdlib only — no extra packages).
- **gh** (GitHub CLI, `gh --version`) — authenticated (`gh auth login`).
- **SDL3** via vcpkg (`vcpkg install sdl3 --triplet x64-windows`).

> Releases are **tagged from `master`**. All work lands on `master` first,
> gated green, and only then is tagged and published.

---

## 1. Build

Configure once (or when options change), then build with the desired
configuration. Debug is for day-to-day work; **Release is for shipping**.

```bash
# Configure, once (Visual Studio multi-config generator):
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
#  (add -DCMAKE_TOOLCHAIN_FILE=...  for vcpkg SDL3)

# Build the configuration you need:
cmake --build build --config Debug   --parallel
cmake --build build --config Release --parallel
```

The engine executable lands at `build/Release/CaesuraAmeKAG.exe`
(and `build/Debug/CaesuraAmeKAG.exe`). Live2D is off by default; add
`-DCAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT="..."` if the SDK is available.

---

## 2. Run the release gates (do not skip)

A release must be fully green locally before tagging. These are the same
gates CI enforces (plus the coupling budget).

### 2.1 Build — zero errors
```bash
cmake --build build --config Release --parallel   # Release for shipping
```

### 2.2 C++ test suite (doctest / CTest)
Run from `build/tests/Debug` — **CWD matters** for resource paths.
```bash
cd build/tests/Debug && ./CaesuraTests.exe
# expect: 0 failed, 0 skipped
```
For the Release gate, the `Release` binaries are built alongside the app (`build/tests/Release/CaesuraTests.exe`);
use that instead when validating a shipping config:
```bash
cd build/tests/Release && ./CaesuraTests.exe
# expect: 0 failed, 0 skipped
```
Or via CTest:
```bash
ctest -C Debug --test-dir build --output-on-failure
```

### 2.3 Lua script suites (from repo root)
```bash
external/lua/lua.exe tests/scripts/run_lua_tests.lua
external/lua/lua.exe tests/scripts/run_orphan_tests.lua
```

### 2.4 Coupling budget
```bash
python scripts/count_coupling.py --ci
```

### 2.5 Doc freshness (generated artifacts)
CI regenerates generated docs and fails on drift. Refresh locally:
```bash
python scripts/api_stats.py               # regenerates docs/api/api-stats.md
python scripts/gen_changelog.py          # regenerates CHANGELOG.md (see §3)
```
If `git status` shows diffs in generated files, include them in the release commit.

---

## 3. Generate the changelog

`scripts/gen_changelog.py` parses Conventional Commits (`type(scope): description`,
types `feat/fix/test/docs/...`) and emits `CHANGELOG.md` at the repo root.
It uses only the Python standard library (works in git bash and PowerShell).

```bash
# Last 100 commits (default):
python scripts/gen_changelog.py

# Last 50 commits:
python scripts/gen_changelog.py --count 50

# Only commits from a date onward:
python scripts/gen_changelog.py --since 2026-08-01

# Commits since a tag (..HEAD):
python scripts/gen_changelog.py --from-tag v1.0.0-alpha

# Title the section for a release version:
python scripts/gen_changelog.py --from-tag v1.0.0-alpha --tag v1.0.1

# Preview without writing:
python scripts/gen_changelog.py --dry-run
```

Output is grouped by semantic type (`Feat` / `Fix` / `Refactor` / `Test` /
`Docs` / `Build` / `Ci` / `Chore` / `Plan`), each entry with its short SHA.
After generation, **hand-polish** the result: merge verbose per-commit entries
into grouped, readable English bullets, drop `plan:` bookkeeping commits, and
order entries by impact. The curated `CHANGELOG.md` is what you commit.

---

## 4. Package with CPack

CPack (ZIP generator) is wired into `CMakeLists.txt` — it installs the
executable, `scripts/`, `demo/`, `assets/`, `shaders/`, `README.md`, `LICENSE`,
and (on Windows) the SDL3 DLL and FFmpeg DLLs.

```bash
# Build Release first, then package:
cmake --build build --config Release --parallel
cd build && cpack -C Release -G ZIP
```

The ZIP is produced in `build/` — the actual name is
`CaesuraAmeKAG-1.0.0-Windows-AMD64.zip` (from `CPACK_PACKAGE_FILE_NAME` =
`CaesuraAmeKAG-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}`,
i.e. `-Windows-AMD64`, not `-win64`).
CPack also emits a companion `CaesuraAmeKAG-1.0.0-Windows-AMD64.zip.sha256` checksum.
The archive root is the versioned folder `CaesuraAmeKAG-1.0.0-Windows-AMD64/`
(not the repo root), so smoke-test paths include that top folder.

---

## 5. Verify the ZIP contents

Before publishing, confirm the archive is complete and runnable:

| Check | Expected |
|-------|----------|
| Executable present | `CaesuraAmeKAG.exe` at the archive root |
| Runtime DLLs present | `SDL3.dll` + `avcodec/avformat/avutil/swscale/swresample-*.dll` (FFmpeg builds) |
| Data dirs present | `scripts/`, `demo/`, `assets/`, `shaders/` |
| Licensing | `README.md` + `LICENSE` at the archive root |
| Smoke test | Extract to a clean folder, launch `CaesuraAmeKAG.exe` from that folder, verify the demo boots without missing-resource errors |

```bash
# List archive contents:
unzip -l build/CaesuraAmeKAG-*.zip

# Extract + smoke test:
mkdir -p /tmp/caesura-smoke && cd /tmp/caesura-smoke
unzip "$OLDPWD/build/CaesuraAmeKAG-*.zip"
ls -la && ./CaesuraAmeKAG.exe --frames 60   # --frames = deterministic GPU smoke run
```

> The engine resolves `assets/` etc. relative to its **working directory**,
> so always launch the exe from the extracted folder, not from elsewhere.
>
> **Expected notes during the smoke run:**
> - A clean boot logs Lua/Render/DevCore/Debug/VFX/MiniGame/AI registration lines and
>   ends with `--frames N` exiting 0. Missing-resource errors would abort instead.
> - Two benign `[WARN] HotReload scan failed: ... "assets/script/"` lines appear because
>   `assets/script/` is a dev-only dir not shipped in the ZIP — not a missing-resource failure.
>
> The ZIP also bundles vendored dependency trees (`include/`, `lib/`, `cmake/` for
> freetype/soloud/zstd) and a `scripts/__pycache__/` — expected CPack artifacts, not errors.

---

## 6. Publish a GitHub release

Use the GitHub CLI. Two flows are supported.

### 6.1 Draft release for a version tag
```bash
# Create an annotated version tag on the release commit:
git tag -a v1.0.1 -m "Caesura (AmeKAG) v1.0.1"
git push origin v1.0.1

# Create the release and attach the ZIP:
gh release create v1.0.1 \
  build/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip \
  --title "Caesura (AmeKAG) v1.0.1" \
  --notes-file CHANGELOG.md \
  --draft
```

Use the **real asset name** `build/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip` (see §4).

> **`gh release create` has no `--dry-run` flag.** To test the publish flow without
> publishing, create the release with `--draft`: it uploads the asset and lets you
> review the page, but doesn't publish. Verify your exact command with
> `gh release create --help` first. (The asset-name and command above were validated
> against a real CPack run and `gh` 2.93.0; no `v1.0.1` tag existed, so nothing was
> published during verification.)

`--draft` lets you review the release page before publishing; drop it when
ready. `--notes-file CHANGELOG.md` uses your curated changelog as the body.

### 6.2 Release from CI artifacts (tag-triggered)
- Push a tag like `v1.0.1` and the CI `release` job (which runs on tags)
  builds and CPack-zips Windows automatically.
- Download the `caesura-amekag-windows-x64` artifact from the workflow run -- it
  is an **upload folder** named `caesura-amekag-windows-x64` (containing the CPack
  ZIP `CaesuraAmeKAG-<ver>-Windows-AMD64.zip`, the exe, and SDL3.dll),
  not a file with that name. Then:
```bash
gh release create v1.0.1 ./releases/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip \
  --title "Caesura (AmeKAG) v1.0.1" --notes-file CHANGELOG.md
```

### 6.3 Manage existing releases
```bash
gh release list                        # list releases
gh release view v1.0.1               # view a release
gh release upload v1.0.1 extra.zip   # add an asset later
gh release delete v1.0.1 --yes       # delete a release (tag kept)
gh release delete-tag v1.0.1 --yes   # delete the tag separately
```

---

## 7. Versioning

- Project version lives in `CMakeLists.txt` (`project(CaesuraAmeKAG VERSION ...)`);
  it flows into the CPack package version and the ZIP name automatically.
- Bump the version before tagging a release (commit the bump, then tag).
- `CHANGELOG.md` sections may be titled with the version (`--tag v1.0.1`) in
  addition to the date.

See also: `docs/guides/getting-started.md` (build/runtime), `AGENTS.md` §12
(docs layout). Commit conventions: `type(scope): description` with types
`feat/fix/test/docs/review/merge/plan`.

---

## Quick reference (copy-paste)

```bash
# gates + docs
cmake --build build --config Release --parallel
cd build/tests/Debug && ./CaesuraTests.exe && cd ../../..
# (a Release-gate may also run the Release tests: cd build/tests/Release && ./CaesuraTests.exe)
external/lua/lua.exe tests/scripts/run_lua_tests.lua
python scripts/count_coupling.py --ci

# changelog + package
python scripts/gen_changelog.py --from-tag v1.0.0-alpha --tag v1.0.1
cmake --build build --config Release --parallel
cd build && cpack -C Release -G ZIP && cd ..

# publish
git tag -a v1.0.1 -m "Caesura (AmeKAG) v1.0.1"
git push origin v1.0.1
gh release create v1.0.1 build/CaesuraAmeKAG-1.0.1-Windows-AMD64.zip --title "Caesura (AmeKAG) v1.0.1" --notes-file CHANGELOG.md --draft
```
