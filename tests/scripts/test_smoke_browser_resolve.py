#!/usr/bin/env python3
"""Contract tests for the browser resolver in scripts/web_browser_smoke.mjs
(Web package CI gate, t144 design C).

`--print-browser` resolves the browser exactly as a smoke run would, prints
the executable path on stdout and exits 0 -- or prints
"[web-smoke] FATAL: browser not found: <label>" on stderr and exits 1 --
without serving anything or launching a browser. It is handled BEFORE the
--root/index.html check so the resolver can be tested on a bare checkout.

Resolution contract under test:
    --browser <path>      a value containing '/' or '\\' is a path: it must
                          exist, no table fallback
    --browser <name>      own-property lookup in CHROME_PATHS (no
                          Object.prototype hits such as 'constructor')
    CHROME_BIN            prepended to the chrome table ONLY when the
                          requested browser is chrome; never overrides an
                          explicit --browser edge

Every case pins --root to a fixture directory that contains an index.html,
so a failure here is about the resolver, not about a missing dist.

SKIP semantics (ctest SKIP_RETURN_CODE 77): node absent -> 77, never 0.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SMOKE = ROOT / "scripts" / "web_browser_smoke.mjs"
SKIP_EXIT = 77
NODE = shutil.which("node")

# A run that ignores --print-browser falls into the real smoke flow and
# would wait on the 150 s boot timeout; cut it off well before that.
RUN_TIMEOUT_S = 40


def run_smoke(*args, env_extra=None, root=None):
    env = dict(os.environ)
    env.pop("CHROME_BIN", None)
    if env_extra:
        env.update(env_extra)
    cmd = [NODE, str(SMOKE), "--print-browser", "--root", str(root)] + [str(a) for a in args]
    try:
        res = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True,
                             encoding="utf-8", errors="replace", timeout=RUN_TIMEOUT_S, env=env)
    except subprocess.TimeoutExpired as exc:
        return None, (exc.stdout or "") if isinstance(exc.stdout, str) else "", "TIMEOUT"
    return res.returncode, res.stdout or "", res.stderr or ""


class ResolverCase(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / "dist"
        self.root.mkdir()
        (self.root / "index.html").write_text("<!doctype html><title>fixture</title>\n", encoding="utf-8")
        self.fake_bin = Path(self.tmp.name) / "fake-chrome"
        self.fake_bin.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def test_default_resolves_to_an_existing_executable_or_says_not_found(self):
        rc, out, err = run_smoke(root=self.root)
        self.assertIn(rc, (0, 1), (rc, out, err))
        if rc == 0:
            self.assertTrue(Path(out.strip()).is_file(), out)
        else:
            self.assertIn("browser not found", err)

    def test_chrome_bin_wins_for_the_default_chrome_request(self):
        rc, out, err = run_smoke(root=self.root, env_extra={"CHROME_BIN": str(self.fake_bin)})
        self.assertEqual(rc, 0, (rc, out, err))
        self.assertEqual(Path(out.strip()), self.fake_bin, out)

    def test_chrome_bin_does_not_override_an_explicit_edge_request(self):
        rc, out, err = run_smoke("--browser", "edge", root=self.root,
                                 env_extra={"CHROME_BIN": str(self.fake_bin)})
        self.assertIn(rc, (0, 1), (rc, out, err))
        self.assertNotEqual(out.strip(), str(self.fake_bin), out)
        if rc == 0:
            self.assertIn("edge", out.lower(), out)
        else:
            self.assertIn("browser not found", err)

    def test_explicit_path_is_used_verbatim(self):
        rc, out, err = run_smoke("--browser", str(self.fake_bin), root=self.root)
        self.assertEqual(rc, 0, (rc, out, err))
        self.assertEqual(Path(out.strip()), self.fake_bin, out)

    def test_missing_explicit_path_is_not_found_without_table_fallback(self):
        rc, out, err = run_smoke("--browser", str(Path(self.tmp.name) / "nope" / "chrome"), root=self.root)
        self.assertEqual(rc, 1, (rc, out, err))
        self.assertIn("browser not found", err)
        self.assertEqual(out.strip(), "", out)

    def test_prototype_property_name_is_not_a_browser(self):
        rc, out, err = run_smoke("--browser", "constructor", root=self.root)
        self.assertEqual(rc, 1, (rc, out, err))
        self.assertIn("browser not found", err)
        self.assertNotIn("TypeError", err)


if __name__ == "__main__":
    if NODE is None:
        print("smoke browser resolver: node not on PATH -> nothing can run; reporting SKIP (77)")
        sys.exit(SKIP_EXIT)
    suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__])
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    ran = result.testsRun
    failed = len(result.failures) + len(result.errors)
    print("")
    print("=" * 70)
    print("smoke browser resolver contract: %d run, %d passed, %d failed"
          % (ran, ran - failed, failed))
    print("=" * 70)
    if failed:
        sys.exit(1)
    print("ALL SMOKE BROWSER RESOLVER CONTRACT TESTS PASSED")
    sys.exit(0)
