#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_caesura_build_cli.py — CLI contract tests for "caesura build" / "caesura package"
(scripts/caesura_build.py, wired into scripts/caesura.py).

Designed to run in the normal gate as the ctest test CaesuraBuildCli
(registered in tests/CMakeLists.txt — see the add_test block under the
Python3_EXECUTABLE guard at the end of that file); the repo CI does not
otherwise execute tests/*.py. NOTE: the registration must exist for this
statement to be true — this file never registers itself.

SKIP semantics (ctest SKIP_RETURN_CODE 77): the engine binary lives under
build/ | bin/, which are gitignored — it is NOT a repo invariant. Everything
that can be verified WITHOUT a binary (error paths, project resolution,
argument parsing, the asset scanner) always runs. The build/package cases and
the ks_check gating case skip when no binary exists (caesura build resolves
the engine BEFORE running ks_check, so the gate is only reachable with an
engine present); the runner then exits 77 and ctest reports SKIP.

Exit precedence is FAIL (1) > SKIP (77) > PASS (0). A green tick must never
mean "packaging verified" on a host that never packaged anything: reporting
PASS while the engine-dependent half silently skipped is exactly the
"skip counted as a pass" miscount this suite exists to prevent.

Layers under test are the REAL ones: every case shells out to
'python scripts/caesura.py ...' exactly as a creator would. No handler is
called directly, so a broken argparse wiring or a broken import cannot pass.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CLI = ROOT / "scripts" / "caesura.py"
SKIP_EXIT = 77

sys.path.insert(0, str(ROOT / "scripts"))
import caesura_build  # noqa: E402


def run_cli(*args, timeout=900):
    """Invoke the real CLI. Returns (rc, combined output)."""
    res = subprocess.run(
        [sys.executable, str(CLI)] + [str(a) for a in args],
        cwd=str(ROOT), capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=timeout,
    )
    return res.returncode, (res.stdout or "") + (res.stderr or "")


def engine_available():
    try:
        return caesura_build.find_engine()
    except caesura_build.BuildError:
        return None


ENGINE = engine_available()


class TestErrorPaths(unittest.TestCase):
    """Actionable diagnostics, never a traceback (no engine binary required)."""

    def test_help_lists_build_and_package(self):
        rc, out = run_cli("--help")
        self.assertEqual(rc, 0, out)
        self.assertIn("build", out)
        self.assertIn("package", out)

    def test_missing_project_is_actionable(self):
        rc, out = run_cli("build", "definitely_not_a_project_9f3a")
        self.assertEqual(rc, 1, out)
        self.assertIn("Project not found", out)
        self.assertIn("Searched", out)
        self.assertIn("caesura.py create", out)      # tells the user what to do
        self.assertNotIn("Traceback", out)

    def test_bad_engine_path_is_actionable(self):
        with tempfile.TemporaryDirectory() as td:
            rc, out = run_cli("build", "basic", "--engine", os.path.join(td, "nope"))
        self.assertEqual(rc, 1, out)
        self.assertIn("--engine", out)
        self.assertNotIn("Traceback", out)

    def test_project_without_scenes_is_actionable(self):
        with tempfile.TemporaryDirectory() as td:
            rc, out = run_cli("build", td)
        self.assertEqual(rc, 1, out)
        self.assertIn("No .ks scenes", out)
        self.assertNotIn("Traceback", out)

    @unittest.skipIf(ENGINE is None, "no engine binary: caesura build resolves the "
                            "engine before ks_check, so the gate is only reachable with an engine")
    def test_ks_check_failure_blocks_and_quotes_the_violation(self):
        with tempfile.TemporaryDirectory() as td:
            # [playbgm] with neither file= nor storage= is a real contract
            # violation (scripts/ks_check.lua), not a synthetic marker.
            (Path(td) / "story.ks").write_text(
                '*start\n[playbgm volume=0.5]\n[ch name="A" text="hi"]\n[end]\n',
                encoding="utf-8")
            rc, out = run_cli("build", td)
        self.assertEqual(rc, 1, out)
        self.assertIn("ks_check", out)
        self.assertIn("playbgm", out)                # the actual violation text
        self.assertIn("--skip-check", out)           # the escape hatch
        self.assertNotIn("Traceback", out)

    def test_engine_missing_diagnostic_shape(self):
        """The no-engine message must name the search list AND the two fixes."""
        try:
            caesura_build.find_engine(explicit=None, config="NoSuchConfig")
        except caesura_build.BuildError as e:
            msg = str(e)
            self.assertIn("Engine binary not found", msg)
            self.assertIn("Searched", msg)
            self.assertIn("CAESURA_ENGINE", msg)
            self.assertIn("cmake --build", msg)
            self.assertIn("doctor", msg)
        else:
            # An engine exists on this host, so the failure branch cannot be
            # exercised end-to-end. Assert that fact rather than pretending the
            # branch was covered.
            self.assertIsNotNone(ENGINE)


class TestAssetScanner(unittest.TestCase):
    """The reference scanner decides whether a package ships holes."""

    def test_scans_storage_sprite_file_attributes(self):
        with tempfile.TemporaryDirectory() as td:
            s = Path(td) / "a.ks"
            s.write_text(
                '[bg storage="assets/bg/x.png"]\n'
                '[ch sprite="assets/fg/y.png" text="hi"]\n'
                '[video file="assets/mov/z.mp4"]\n'
                '[bg storage="assets/bg/x.png"]\n'          # duplicate
                '[bg storage="../escape.png"]\n',           # traversal: rejected
                encoding="utf-8")
            refs = caesura_build.scan_asset_refs([s])
        self.assertEqual(refs, ["assets/bg/x.png", "assets/fg/y.png", "assets/mov/z.mp4"])

    def test_runtime_computed_paths_are_not_reported_as_missing(self):
        """A macro-parameter path (demo/example_game/story.ks:16) is not static."""
        with tempfile.TemporaryDirectory() as td:
            s = Path(td) / "a.ks"
            s.write_text('[bg storage="assets/bg/%bg%"]\n'
                         '[bg storage="assets/bg/real.png"]\n', encoding="utf-8")
            static = caesura_build.scan_asset_refs([s])
            dynamic = caesura_build.scan_dynamic_asset_refs([s])
        self.assertEqual(static, ["assets/bg/real.png"])
        self.assertEqual(dynamic, ["assets/bg/%bg%"])

    def test_missing_reference_is_reported_not_swallowed(self):
        with tempfile.TemporaryDirectory() as td:
            proj = Path(td) / "p"
            out = Path(td) / "o"
            proj.mkdir()
            out.mkdir()
            copied, missing = caesura_build.resolve_referenced_assets(
                ["assets/bg/classroom.png", "assets/bg/__no_such_asset__.png"], proj, out)
        self.assertIn("assets/bg/classroom.png", copied)     # pulled from repo pool
        self.assertIn("assets/bg/__no_such_asset__.png", missing)


@unittest.skipIf(ENGINE is None, "no engine binary (build/ is gitignored)")
class TestGameOnlyBuild(unittest.TestCase):
    """Full build against the real engine binary and the stock basic template."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="caesura-build-test-")
        cls.out = Path(cls.tmp) / "basic-game"
        cls.rc, cls.log = run_cli("build", "basic", "-o", str(cls.out))

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_build_succeeds(self):
        self.assertEqual(self.rc, 0, self.log)

    def test_engine_and_runtime_libs_are_beside_the_binary(self):
        exe = self.out / caesura_build._exe_name()
        self.assertTrue(exe.is_file(), self.log)
        if os.name == "nt":
            # A player must not copy DLLs by hand: SDL3 has to be in the folder.
            self.assertTrue((self.out / "SDL3.dll").is_file(),
                            sorted(p.name for p in self.out.iterdir()))

    def test_engine_required_assets_present(self):
        # No font => no text at all (src/entry/Engine.cpp:366).
        fonts = list((self.out / "assets" / "fonts").glob("*.otf"))
        self.assertTrue(fonts, "packaged assets/fonts is empty")
        self.assertTrue((self.out / "assets" / "lang").is_dir())

    def test_game_lives_under_the_sandbox_allowlisted_root(self):
        # scripts/sandbox.lua:314-316 allowlists projects/ for post-lockdown
        # io.open; a game outside it cannot cross-scene [jump].
        self.assertTrue((self.out / "projects" / "basic" / "story.ks").is_file())

    def test_runtime_config_points_at_the_generated_boot_shim(self):
        cfg = (self.out / "scripts" / "config.lua").read_bytes()
        self.assertIn(b"projects/basic/caesura-boot.lua", cfg)
        self.assertIn(b"config.dev_mode = false", cfg)      # release default

    def test_boot_shim_installs_frame_hooks(self):
        # tools/project_templates/*/entry.lua define NO engine_update /
        # engine_render, so without these the story loads and never advances.
        shim = (self.out / "projects" / "basic" / "caesura-boot.lua").read_text(encoding="utf-8")
        for hook in ("engine_update", "engine_render", "_KAG_onClick"):
            self.assertIn(hook, shim)

    def test_referenced_assets_resolved(self):
        info = json.loads((self.out / "BUILD-INFO.json").read_text(encoding="utf-8"))
        self.assertEqual(info["kind"], "caesura-game-only")
        self.assertEqual(info["assets_missing"], [], info)
        self.assertGreater(info["asset_refs"], 0)
        for ref in caesura_build.scan_asset_refs(
                sorted((self.out / "projects" / "basic").rglob("*.ks"))):
            self.assertTrue((self.out / ref).is_file(), "missing packaged asset: %s" % ref)

    def test_scenes_are_precompiled_into_the_ksc_cache(self):
        # A cold compile of a large scene blows the 2,000,000-instruction
        # startup budget (src/script/vm/LuaManager.cpp:63) -- reproduced with
        # demo/example_game/story.ks, which only booted once cache/ksc existed.
        info = json.loads((self.out / "BUILD-INFO.json").read_text(encoding="utf-8"))
        self.assertEqual(info["precompile_failures"], [], info)
        cached = list((self.out / "cache" / "ksc").glob("*.ksc"))
        self.assertTrue(cached, "cache/ksc is empty: the player pays a cold compile at boot")
        self.assertEqual(len(info["precompiled_scenes"]), len(info["scenes"]), info)

    def test_no_developer_tooling_shipped(self):
        stray = [p.name for p in (self.out / "scripts").rglob("*")
                 if p.is_file() and p.suffix.lower() in caesura_build.DEV_SCRIPT_SUFFIXES]
        self.assertEqual(stray, [], "dev-only files leaked into the player package")

    def test_package_does_not_reference_the_repo(self):
        """Nothing inside the package may point back at the source checkout."""
        needle = str(ROOT).replace("\\", "/").lower()
        offenders = []
        for p in (self.out / "scripts" / "config.lua",
                  self.out / "projects" / "basic" / "caesura-boot.lua"):
            if needle in p.read_text(encoding="utf-8", errors="replace").replace("\\", "/").lower():
                offenders.append(p.name)
        self.assertEqual(offenders, [])


@unittest.skipIf(ENGINE is None, "no engine binary (build/ is gitignored)")
class TestDesktopPackage(unittest.TestCase):
    """caesura package --target windows produces a self-contained archive."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="caesura-pkg-test-")
        cls.rc, cls.log = run_cli("package", "basic", "--target", "windows", "-o", cls.tmp)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_archive_created(self):
        self.assertEqual(self.rc, 0, self.log)
        zips = list(Path(self.tmp).glob("*.zip"))
        self.assertEqual(len(zips), 1, [p.name for p in Path(self.tmp).iterdir()])

    def test_archive_has_single_top_level_dir_with_engine_and_game(self):
        zips = list(Path(self.tmp).glob("*.zip"))
        self.assertTrue(zips, self.log)
        with zipfile.ZipFile(zips[0]) as zf:
            names = zf.namelist()
        tops = {n.split("/")[0] for n in names}
        self.assertEqual(len(tops), 1, tops)              # never unzips into CWD
        self.assertTrue(any(n.endswith(caesura_build._exe_name()) for n in names))
        self.assertTrue(any("projects/basic/story.ks" in n for n in names))
        self.assertTrue(any("assets/fonts/" in n for n in names))


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    ran = result.testsRun
    skipped = len(result.skipped)
    failed = len(result.failures) + len(result.errors)
    print("")
    print("=" * 70)
    print("caesura build/package CLI: %d run, %d passed, %d failed, %d skipped"
          % (ran, ran - failed - skipped, failed, skipped))
    if ENGINE is None:
        print("engine binary: NOT FOUND -> build/package cases SKIPPED "
              "(build/ and bin/ are gitignored; not a repo invariant)")
    else:
        print("engine binary: %s" % ENGINE)
    print("=" * 70)
    if failed:
        sys.exit(1)
    # SKIP semantics: with no engine binary, the engine-dependent half of the
    # suite (TestGameOnlyBuild, TestDesktopPackage, ks_check gating) did not
    # run. Report SKIP (77) to ctest, never PASS -- a green tick must not mean
    # "packaging verified" on a host that never packaged anything. (The old
    # predicate 'ran - skipped == 0' could not trip in a partial-skip run: the
    # engine-independent cases always execute, so a skipped engine half was
    # counted as a pass.)
    if ENGINE is None:
        print("caesura build/package CLI: %d passed, %d skipped; reporting SKIP "
              "(77) -- no engine binary on this host" % (ran - failed - skipped, skipped))
        sys.exit(SKIP_EXIT)
    if skipped:
        print("NOTE: %d case(s) skipped." % skipped)
    print("ALL CAESURA BUILD CLI TESTS PASSED")
    sys.exit(0)
