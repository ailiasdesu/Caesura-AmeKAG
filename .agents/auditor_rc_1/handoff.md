# Forensic Integrity Audit Report — Caesura (AmeKAG) 1.x Release Candidate

**Work Product**: Caesura (AmeKAG) 1.x Release Candidate (`1.0.0-rc.1` @ commit `62132e783dd238752659d4227ff26b0235258ea9`)  
**Profile**: General Project (Integrity Forensics)  
**Auditor**: Forensic Auditor (`auditor_rc_1`)  
**Verdict**: **`CLEAN`**

---

## 1. Observation

Direct empirical observations from tool execution, file reviews, and cryptographic audits:

1. **Static Analysis of Verification Scripts**:
   - `scripts/verify_release_candidate.py` (lines 62–70, 584–793): Implements `compute_sha256`, reads and parses `manifest.json`, dynamically checks all 9 blockers in `REQUIRED_BLOCKERS`, verifies all 20 checksums against disk files, validates parity files for `verified`/`hardware-gated` statuses, checks existence and non-zero byte size of all 12 report files, and ensures `docs/status/release-candidate-report.md` contains unambiguous `RC-GO` declarations.
   - `scripts/compare_platform_parity.py` (lines 83–148, 179–338): Implements recursive AST/dictionary leak detection against forbidden keys (`gpu`, `vram`, `fps`, `hwnd`, `pid`, `thread_id`, `renderer_type`) and forbidden path/pointer values (`C:\`, `/home/`, `/Users/`, `0x...`), validates `FirstVNStateSnapshot` schema, checks route A/B outcome against `CANONICAL_ROUTE_A`/`CANONICAL_ROUTE_B`, and enforces cross-platform state equivalence across all verified platforms.
   - `scripts/verify_android_regression.py` (lines 45–249): Performs 88 distinct programmatic checks inspecting `AndroidManifest.xml`, `MainActivity.java`, `TextRenderer.cpp` (2048x2048 RGBA8 atlas with 8,074 glyphs), `BgfxQuadBatch.cpp` (transient vertex/index buffers and `MergeGroup`), `RenderBinding.cpp` (RTT vs Texture ID namespace isolation), `Engine.cpp` (touch scaling & gesture integration), `SaveManager.cpp` (system slots `-1`/`-2` within `-2..99`), `SoLoudAudioEngine.cpp` (3-bus audio), `IPlatformBackend.h`/`SDL3PlatformBackend.cpp`/`DevCoreBinding.cpp` (IME text input bridge), and `android/app/build.gradle` (release signing configs).
   - `scripts/generate_platform_status.py` (lines 89–205): Enforces strict 7-enum status constraints, validates that every `verified` capability contains valid commit SHA, existing documentation path, test command, and timestamp, and enforces that `ios.real_device` must remain `hardware-gated`.
   - **Result**: Zero dummy bypasses, zero facade implementations, and zero hardcoded test returns across all scripts.

2. **Data & Cryptographic Integrity**:
   - Running independent SHA-256 verification against `artifacts/release/checksums/sha256sums.txt`:
     ```text
     Total checksum lines: 20
     [OK] artifacts/release/manifest.json -> 2b9f86dd5794c7519d9d2d841598c6b44c0d3f194846505cc5e6f4d20774b342
     [OK] artifacts/release/platform-status.json -> 8d847bb04a968551eae91aacb82180d160dfa9473150724e44e24ac0372a9000
     [OK] artifacts/release/parity/windows.json -> 2379700b7d52475450a60879e92a3e3a31b0c0c1cd464d55453ff3aefe122231
     [OK] artifacts/release/parity/linux.json -> 55d452d5244421206bb6e76f0d50a55b19df3e93f832af29e30a367b8e359de8
     [OK] artifacts/release/parity/web.json -> 0ebfb3a29126e51e3ee1152d04c7ffbc7da65239e48c10208bc33c94d4c4b298
     [OK] artifacts/release/parity/android.json -> b04c23efb5152f882aefb339cfa69e238857c9614ef6c1a34c4d38682331a821
     [OK] artifacts/release/parity/ios.json -> 495be6020d02c59ba531ca7df8d604b7f0ea08042250783e8d95749e33736cac
     [OK] artifacts/release/parity/parity_summary.json -> 032cd858c847a5b12cdd7ca3c656ae68fca02073b804f8a4cf0c61ccab6ff8bb
     [OK] artifacts/release/reports/cpp_test_report.md -> 5a558a3af93867dda9761f3638fb1a83dedc47c7eee822d71fd96cb43b155bbc
     [OK] artifacts/release/reports/cpp_test_report.json -> 0f8e1e5c9cf404831e35d8905e41f8da3da9077796753a394adc0022b8112871
     [OK] artifacts/release/reports/lua_test_report.md -> e48e070c564bdca598a9db975dc96d3b19e3dfd212ce440517025fa96ce8d98e
     [OK] artifacts/release/reports/lua_test_report.json -> d4116ef5120cd2ed2696d62a971d6c51c21e0f14fb08b6d8313b50bc17d81c22
     [OK] artifacts/release/reports/coupling_report.md -> 354172274fbdb64e75d26fd7bb492266af242be49a64a110e3dcf98472c309dd
     [OK] artifacts/release/reports/coupling_report.json -> 65d92b1b21df5c280b101f42ec15304570db812cdf516af9d33e25d64673e0c1
     [OK] artifacts/release/reports/metal_shader_report.md -> 1563747cfcee9e0380de8fe108856e91600a318108ab251d3087ce6b766fdeb4
     [OK] artifacts/release/reports/metal_shader_report.json -> 713aa13d697588e9123eeb9ec319f4846c4e815da541e12bc3da39c1dbb7c9f3
     [OK] artifacts/release/reports/android_regression_report.md -> 4b60b39fcc983bdeebf1c87e00275f5d427557e1368b13164ed795406edec819
     [OK] artifacts/release/reports/android_regression_report.json -> a6ca16c48e26264fbbd45837107e39ec0ec340cbcfe758ae539791d59f054aa8
     [OK] artifacts/release/reports/parity_report.md -> 582ef7d1ffc5914c76a28357235d6ac1a6984cb5df9a03ae61c0034bd77e62cc
     [OK] artifacts/release/reports/parity_report.json -> 07dcf8e07685423b64298ae1b3260c1ed937c3f952f5188453a5e152cbb701c2
     Independently Verified: 20/20, Mismatches: 0
     ```
   - Parity snapshots in `artifacts/parity/` and `artifacts/release/parity/` evaluated with `sanitize_check_obj`: 0 errors, 0 leaks, 0 route mismatches across Windows, Linux, Web, Android, and iOS.

3. **Empirical Test Suite Execution**:
   - `build/tests/Debug/CaesuraTests.exe`:
     `[doctest] test cases: 1052 | 1052 passed | 0 failed | 0 skipped`
     `[doctest] assertions: 385299 | 385299 passed | 0 failed |`
     `[doctest] Status: SUCCESS!`
   - `build/lua/Debug/lua.exe tests/scripts/run_lua_tests.lua`:
     `Results: 134 passed, 0 failed, 134 total`
   - `build/lua/Debug/lua.exe tests/scripts/run_orphan_tests.lua`:
     `Results: 24 passed, 0 failed, 24 total`
     Total Lua Suites: 158/158 passed (100% pass rate).
   - `python scripts/count_coupling.py`:
     16/16 modules within architectural `#include` coupling limits (`entry`: 14/14, `di`: 13/14, `script`: 11/14, others <= 4).
   - `python scripts/verify_android_regression.py`:
     `Summary: 88 Passed, 0 Failed out of 88 checks.`
   - `python scripts/verify_metal_shaders.py`:
     `PASS: All 12 Metal shader assets and fallback pathways verified.`
   - `python tests/scripts/test_platform_parity.py`:
     `Ran 10 tests in 0.151s - OK`
   - `python scripts/generate_platform_status.py --check`:
     `[OK] Platform status matrix is valid and 'docs/status/platform-status.md' is up-to-date.`
   - `python scripts/verify_release_candidate.py --check`:
     `[SUCCESS] All release candidate gate conditions and evidence assets verified. GATE DECISION: RC-GO`

4. **Workspace & Layout Compliance**:
   - `find_by_name` on `.agents/` excluding `skills/` for source/test/binary extensions returned 0 results. `.agents/` contains solely agent coordination metadata.

---

## 2. Logic Chain

1. **Verification Logic Authenticity**:
   - All validation scripts (`verify_release_candidate.py`, `compare_platform_parity.py`, `verify_android_regression.py`, `generate_platform_status.py`, `verify_metal_shaders.py`) were inspected at the source code level.
   - None of the scripts use early hardcoded returns (`return True`), mocked passes, or bypass flags. Every check performs genuine disk reads, hash computations, AST regex scans, or data comparisons.
   - Unit tests (`tests/scripts/test_platform_parity.py`) prove that `compare_platform_parity.py` actively and correctly fails on corrupted schemas, data leaks, and route divergence.

2. **Cryptographic Consistency & Immutability**:
   - The 20 SHA-256 hashes published in `artifacts/release/checksums/sha256sums.txt` correspond exactly to the current byte contents of all files in `artifacts/release/` (manifest, platform status, parity snapshots, and reports).
   - No hashes are fabricated, mismatched, or outdated.

3. **Subsystem Behavior & Parity**:
   - Parity snapshots for Windows, Linux, Web, and Android record 100% identical route branching (`route_a`: `choice=sun, route=sun, flag_is_sun=1, ending=sunset`; `route_b`: `choice=rain, route=rain, flag_is_sun=0, ending=rain_shelter`), multilingual support (`zh`, `en`, `ja`), and save roundtrip fidelity.
   - iOS is recorded honestly as `hardware-gated` in accordance with Iron Rule 10 without fabricated device claims.
   - Data sanitization rules confirm zero leakage of pointers, absolute local paths, or hardware-specific identifiers into parity state snapshots.

4. **Zero Tolerance Baseline Quality**:
   - Full C++ test suite (1052 doctest cases, 385,299 assertions) and Lua test suite (158 suites) execute to 100% completion with 0 failures and 0 skips.
   - 16/16 architectural module coupling limits are strictly respected.
   - All 9 release blockers are verified cleared with concrete evidence.

---

## 3. Caveats

No caveats. All checks across R1 to R5 were executed and verified directly on the local filesystem and runtime environment.

---

## 4. Conclusion

The Caesura (AmeKAG) 1.x Release Candidate (`1.0.0-rc.1`) work product contains **zero integrity violations**, **zero fake passes**, **zero cryptographic mismatches**, and **zero architectural boundary breaches**.

**Final Verdict**: **`CLEAN`**

---

## 5. Verification Method

To independently reproduce and verify this audit:

```powershell
# 1. Master Release Candidate Gate Verifier
python scripts/verify_release_candidate.py --check

# 2. Independent Cryptographic SHA-256 Hash Verification (20/20)
python -c "
import hashlib
from pathlib import Path
root = Path('.')
lines = (root / 'artifacts/release/checksums/sha256sums.txt').read_text(encoding='utf-8').strip().splitlines()
for l in lines:
    exp, p = l.split(maxsplit=1)
    act = hashlib.sha256((root / p).read_bytes()).hexdigest()
    assert act.lower() == exp.lower(), f'Mismatch for {p}'
print('All 20 SHA-256 hashes matched!')
"

# 3. Platform Status Matrix Schema & Freshness Verification
python scripts/generate_platform_status.py --check

# 4. First-VN Behavioral Parity Comparator & Unit Tests
python scripts/compare_platform_parity.py --dir artifacts/release/parity --summary artifacts/release/parity/parity_summary.json
python tests/scripts/test_platform_parity.py

# 5. Core Test Suites
build/tests/Debug/CaesuraTests.exe
build/lua/Debug/lua.exe tests/scripts/run_lua_tests.lua
build/lua/Debug/lua.exe tests/scripts/run_orphan_tests.lua

# 6. Android & Metal Shaders Validation
python scripts/verify_android_regression.py
python scripts/verify_metal_shaders.py

# 7. Architectural Coupling Compliance
python scripts/count_coupling.py
```
