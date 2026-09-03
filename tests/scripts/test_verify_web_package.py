#!/usr/bin/env python3
"""Contract tests for scripts/verify_web_package.sh (Web package CI gate, t144
design A).

The verifier asserts the shape of a dist/<game>/ tree produced by
scripts/package_game.sh. These tests build a MINIMAL synthetic package under a
temp dir, then break it one property at a time and check that the verifier
goes red loudly (exit 1, naming the offender) rather than staying green.

Exit-code contract under test:
    0  every assertion passed
    1  at least one assertion failed
    2  no package directory to verify
    77 no package directory AND --skip-if-missing (ctest SKIP convention)

SKIP semantics (ctest SKIP_RETURN_CODE 77): the verifier is bash and one of
its assertions shells out to node (web/gen-index.mjs). With either tool
missing from the host nothing here can run, so the runner exits 77 -- never 0
-- so a green tick cannot mean "verified" on a host that verified nothing.
The optional real-package case (package tests/projects/first_vn against an
already-built web/dist) is skipped individually when its inputs are absent;
that skip does NOT turn the whole run into 77 because the synthetic cases
still executed.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERIFY = ROOT / "scripts" / "verify_web_package.sh"
PACKAGE = ROOT / "scripts" / "package_game.sh"
GEN_INDEX = ROOT / "web" / "gen-index.mjs"
SKIP_EXIT = 77

NODE = shutil.which("node")


def _is_windows_system_bash(path):
    """Return true for the WSL launcher exposed as System32/bash.exe."""
    if not path:
        return False
    normalized = str(path).replace("\\", "/").lower()
    return "/windows/system32/" in normalized


def _find_windows_path_bash(path_value):
    for raw_entry in path_value.split(";"):
        entry = raw_entry.strip().strip('"')
        if not entry:
            continue
        candidate = Path(entry) / "bash.exe"
        if candidate.is_file() and not _is_windows_system_bash(candidate):
            return str(candidate)
    return None


def find_git_bash():
    """Resolve Git Bash without allowing Windows' WSL launcher to win."""
    override = os.environ.get("CAESURA_BASH", "").strip()
    if override and Path(override).is_file():
        return override
    if os.name != "nt":
        return shutil.which("bash")

    candidates = []
    for variable in ("ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"):
        root = os.environ.get(variable, "").strip()
        if root:
            candidates.extend((
                Path(root) / "Git" / "bin" / "bash.exe",
                Path(root) / "Git" / "usr" / "bin" / "bash.exe",
            ))
    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if local_app_data:
        candidates.extend((
            Path(local_app_data) / "Programs" / "Git" / "bin" / "bash.exe",
            Path(local_app_data) / "Programs" / "Git" / "usr" / "bin" / "bash.exe",
        ))

    for candidate in candidates:
        if Path(candidate).is_file():
            return str(candidate)

    return _find_windows_path_bash(os.environ.get("PATH", ""))


BASH = find_git_bash()


def posix(p):
    """git-bash on Windows accepts C:/x/y but mangles C:\\x\\y inside scripts."""
    return str(p).replace("\\", "/")


def run_verify(*args, timeout=300):
    res = subprocess.run(
        [BASH, posix(VERIFY)] + [posix(a) if isinstance(a, Path) else str(a) for a in args],
        cwd=str(ROOT), capture_output=True, text=True,
        encoding="utf-8", errors="replace", timeout=timeout,
    )
    return res.returncode, (res.stdout or "") + (res.stderr or "")


GAME = "fixture_vn"
STORY_LUA = 'return {["assets"]={"assets/bg/x.png"},["scenes"]={["story.ks"]={["flow"]={}}}}\n'
INDEX_HTML = (
    '<!doctype html><html><head>'
    '<script>self.__CAESURA_WASM_FILE__ = new URL("web-assets/glue.wasm", document.baseURI).href</script>'
    '<script type="module" src="./web-assets/index-abc123.js"></script>'
    '</head><body><div id="stage"></div></body></html>\n'
)


def make_dist(tmp, story_lua=STORY_LUA, index_html=INDEX_HTML):
    """Smallest tree that must satisfy every verifier assertion."""
    dist = Path(tmp) / GAME
    (dist / "web-assets").mkdir(parents=True)
    (dist / "scripts" / "kag").mkdir(parents=True)
    (dist / "assets" / "bg").mkdir(parents=True)
    (dist / "cache" / "story").mkdir(parents=True)
    (dist / "demo" / GAME).mkdir(parents=True)

    (dist / "index.html").write_text(index_html, encoding="utf-8")
    (dist / "web-assets" / "index-abc123.js").write_text("console.log('player')\n", encoding="utf-8")
    (dist / "web-assets" / "glue.wasm").write_bytes(b"\0asm\1\0\0\0")
    (dist / "scripts" / "kag" / "init.lua").write_text("return {}\n", encoding="utf-8")
    (dist / "scripts" / "tokenizer.lua").write_text("return {}\n", encoding="utf-8")
    (dist / "assets" / "bg" / "x.png").write_bytes(b"\x89PNG\r\n\x1a\n")
    (dist / "cache" / "story" / "story.lua").write_text(story_lua, encoding="utf-8")
    (dist / "demo" / GAME / "story.ks").write_text("*start\n[end]\n", encoding="utf-8")

    # The packaged scripts index must be byte-identical to what gen-index
    # produces for the packaged tree -- generate it with the real tool.
    subprocess.run(
        [NODE, str(GEN_INDEX), str(dist / "scripts"), str(dist / "scripts" / "index.json")],
        cwd=str(ROOT), check=True, capture_output=True,
    )

    listing = "\n".join(
        "%d\t%s" % (p.stat().st_size, p.relative_to(dist).as_posix())
        for p in sorted(dist.rglob("*")) if p.is_file()
    )
    (dist / "MANIFEST.txt").write_text(
        "Caesura (AmeKAG) web package: %s\nbuilt: fixture\nscenes: 1\n---\n"
        "files (size bytes, path):\n%s\n---\ntotal KB: 1\n" % (GAME, listing),
        encoding="utf-8",
    )
    return dist


class TestGoodPackage(unittest.TestCase):

    def test_minimal_package_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = run_verify(make_dist(tmp))
            self.assertEqual(rc, 0, out)
            self.assertIn("FAIL 0", out)

    def test_text_only_vn_with_empty_asset_list_passes(self):
        story = 'return {["assets"]={},["scenes"]={["story.ks"]={["flow"]={}}}}\n'
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = run_verify(make_dist(tmp, story_lua=story))
            self.assertEqual(rc, 0, out)
            self.assertIn("FAIL 0", out)

    def test_verifier_package_path_with_unicode_parentheses_pass(self):
        # The repository itself has both characters in its Windows path, and
        # this temp prefix makes the package argument exercise the same shell
        # quoting boundary instead of relying on an ASCII-only temp path.
        with tempfile.TemporaryDirectory(prefix="Caesura(AmeKAG)_中文_") as tmp:
            rc, out = run_verify(make_dist(tmp))
            self.assertEqual(rc, 0, out)
            self.assertIn("FAIL 0", out)


class TestBashResolution(unittest.TestCase):

    def test_runner_does_not_select_windows_system_bash(self):
        self.assertIsNotNone(BASH, "a bash interpreter is required by this suite")
        if os.name == "nt":
            self.assertFalse(_is_windows_system_bash(BASH), BASH)

    def test_path_scan_skips_system32_and_finds_later_portable_git(self):
        with tempfile.TemporaryDirectory() as tmp:
            system_bin = Path(tmp) / "Windows" / "System32"
            portable_bin = Path(tmp) / "PortableGit" / "bin"
            system_bin.mkdir(parents=True)
            portable_bin.mkdir(parents=True)
            (system_bin / "bash.exe").write_bytes(b"")
            (portable_bin / "bash.exe").write_bytes(b"")

            resolved = _find_windows_path_bash(
                "%s;%s" % (system_bin, portable_bin))
            self.assertEqual(resolved, str(portable_bin / "bash.exe"))


class TestBrokenPackageGoesRed(unittest.TestCase):

    def _assert_red(self, dist, offender):
        rc, out = run_verify(dist)
        self.assertEqual(rc, 1, out)
        self.assertIn("[FAIL]", out)
        self.assertIn(offender, out)

    def test_missing_story_bundle(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            (dist / "cache" / "story" / "story.lua").unlink()
            self._assert_red(dist, "cache/story/story.lua")

    def test_stale_scripts_index(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            with open(dist / "scripts" / "index.json", "a", encoding="utf-8") as fh:
                fh.write("\n")
            self._assert_red(dist, "index.json")

    def test_javascript_leaked_into_game_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            (dist / "assets" / "x.js").write_text("// stray\n", encoding="utf-8")
            self._assert_red(dist, "assets/")

    def test_missing_local_wasm(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            (dist / "web-assets" / "glue.wasm").unlink()
            self._assert_red(dist, "glue.wasm")

    def test_missing_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            (dist / "MANIFEST.txt").unlink()
            self._assert_red(dist, "MANIFEST.txt")

    def test_absolute_web_assets_reference_breaks_subpath_hosting(self):
        html = INDEX_HTML.replace("./web-assets/", "/web-assets/")
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp, index_html=html)
            self._assert_red(dist, "web-assets/")

    def test_story_references_asset_missing_from_package(self):
        with tempfile.TemporaryDirectory() as tmp:
            dist = make_dist(tmp)
            (dist / "assets" / "bg" / "x.png").unlink()
            self._assert_red(dist, "assets/bg/x.png")


class TestNothingToVerify(unittest.TestCase):

    def test_missing_dir_is_a_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = run_verify(Path(tmp) / "does_not_exist")
            self.assertEqual(rc, 2, out)

    def test_missing_dir_with_skip_flag_reports_skip(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out = run_verify(Path(tmp) / "does_not_exist", "--skip-if-missing")
            self.assertEqual(rc, SKIP_EXIT, out)


def real_package_inputs_present():
    if not (ROOT / "web" / "dist" / "index.html").is_file():
        return False
    lua_dirs = [ROOT / "external" / "lua"] + list((ROOT / "build" / "lua").glob("*"))
    return any((d / n).is_file() for d in lua_dirs for n in ("lua.exe", "lua"))


class TestRealPackage(unittest.TestCase):

    @unittest.skipUnless(real_package_inputs_present(),
                         "needs web/dist (cd web && npm run build) and a lua interpreter")
    def test_first_vn_packaged_by_package_game_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "first_vn"
            res = subprocess.run(
                [BASH, posix(PACKAGE), "--no-web-build", "--out", posix(out_dir),
                 "tests/projects/first_vn"],
                cwd=str(ROOT), capture_output=True, text=True,
                encoding="utf-8", errors="replace", timeout=600,
            )
            self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
            rc, out = run_verify(out_dir)
            self.assertEqual(rc, 0, out)
            self.assertIn("FAIL 0", out)


if __name__ == "__main__":
    missing = [name for name, tool in (("bash", BASH), ("node", NODE)) if tool is None]
    if missing:
        print("verify_web_package: %s not on PATH -> nothing can run; reporting SKIP (77)"
              % ", ".join(missing))
        sys.exit(SKIP_EXIT)
    suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    ran = result.testsRun
    skipped = len(result.skipped)
    failed = len(result.failures) + len(result.errors)
    print("")
    print("=" * 70)
    print("verify_web_package contract: %d run, %d passed, %d failed, %d skipped"
          % (ran, ran - failed - skipped, failed, skipped))
    print("=" * 70)
    if failed:
        sys.exit(1)
    if skipped:
        print("NOTE: %d case(s) skipped (real-package case needs web/dist + lua)." % skipped)
    print("ALL VERIFY_WEB_PACKAGE CONTRACT TESTS PASSED")
    sys.exit(0)
