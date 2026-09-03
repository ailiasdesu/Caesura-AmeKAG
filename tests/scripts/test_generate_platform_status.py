#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_generate_platform_status.py — Track B regression suite: git-HEAD drift gate
for scripts/generate_platform_status.py (t169).

Covers the B1 contract (evidence-HEAD baseline: the drift baseline is the
newest commit touching any path OUTSIDE docs/):
  S1  synced:  --check --head <sha == yaml head_commit>               -> exit 0
  S2  drift:   --check with evidence HEAD != yaml head_commit        -> exit 1,
               stderr carries the lag description + the fix hint
               (S2b pins the evidence HEAD so the case is deterministic)
  S3  --head wins over a differing evidence HEAD                     -> exit 0
  S4  no git:  [WARN] fallback to the yaml value, drift skipped, the
               md byte-freshness comparison still runs (fresh -> 0;
               tampered -> 1 with the "is stale or modified" message)
  S5  library parity: generate_markdown(data) without head args embeds
               the same evidence HEAD the CLI would (yaml on no-git hosts)
  S6  --json output still carries the raw yaml head_commit (unchanged)
  S8  real temp repo: a sync commit that ONLY touches docs/ does not
               drift; a later code-only commit does (evidence HEAD moved)

Run directly:  python tests/scripts/test_generate_platform_status.py

No pytest registration and no ctest wiring needed -- this repo never
auto-discovers tests/*.py (the adversarial sibling suite is registered in
tests/CMakeLists.txt as CaesuraPlatformMatrixAdversarial; this file is the
git-HEAD-focused complement and is executed by hand in this batch).

The suite touches only a temp dir; docs/status/ is never read or written.
The real repository git HEAD is reached through the generator's own
subprocess call (or pinned with unittest.mock so runs are deterministic).
"""

import contextlib
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "scripts"))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent))

import generate_platform_status as gps  # noqa: E402

# 8-char style sha, mirroring the real matrix's 93bd5c33; used as the yaml value.
FAKE_YAML_HEAD = "93bd5c33"
# A 40-char pinned "current HEAD" for deterministic git mocks.
PINNED_GIT_HEAD = "3ee1ebaddecd320068df8aa6583bb4075012c859"

# Minimal but SCHEMA-VALID matrix. Note: the generator's validate_matrix()
# requires ALL SIX platform keys (windows/linux/web/android/macos/ios) plus a
# non-empty capabilities dict per platform, so each platform gets the smallest
# legal block; evidence documents must exist in the real repo ("README.md").
MINIMAL_YAML = f"""\
version: 1
schema_version: "1.0.0"
generated_document: "platform-status.md"
last_updated: "2026-09-03T01:42:00Z"
head_commit: "{FAKE_YAML_HEAD}"

allowed_status_enums:
  - verified
  - probe
  - pending
  - hardware-gated
  - credential-gated
  - blocked
  - not-applicable

platforms:
  windows:
    display_name: "Windows (x64)"
    tier: 1
    summary_status: verified
    capabilities:
      build:
        status: verified
        evidence:
          commit: "62132e78"
          document: "README.md"
          test: "cmake --build build --config Debug --parallel"
          verified_at: "2026-08-25T01:57:33Z"
  linux:
    display_name: "Linux (x64)"
    tier: 1
    summary_status: verified
    capabilities:
      build:
        status: verified
        evidence:
          commit: "11111111"
          document: "README.md"
          test: "cmake --build build"
          verified_at: "2026-08-25T01:57:33Z"
  web:
    display_name: "Web (Browser)"
    tier: 1
    summary_status: probe
    capabilities:
      build:
        status: probe
        evidence:
          commit: "22222222"
          document: "README.md"
  android:
    display_name: "Android"
    tier: 1
    summary_status: probe
    capabilities:
      build:
        status: probe
        evidence:
          commit: "33333333"
          document: "README.md"
  macos:
    display_name: "macOS"
    tier: 2
    summary_status: verified
    capabilities:
      build:
        status: verified
        evidence:
          commit: "44444444"
          document: "README.md"
          test: "cmake --build build"
          verified_at: "2026-08-25T01:57:33Z"
  ios:
    display_name: "iOS"
    tier: 2
    summary_status: hardware-gated
    capabilities:
      build:
        status: probe
        evidence:
          commit: "55555555"
          document: "README.md"
"""

_ORIG_SUBPROCESS_RUN = subprocess.run


class _GitResult:
    """Minimal stand-in for a CompletedProcess with the fields the bot reads."""

    def __init__(self, returncode: int, stdout: str):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = ""


# The exact argv the generator must use for the evidence HEAD (never through a
# shell): newest commit touching any path outside docs/.
GIT_LOG_EVIDENCE_CMD = ["git", "log", "-1", "--format=%H", "--", ".", ":(exclude)docs"]


def git_head_pinned(stdout: str = PINNED_GIT_HEAD, returncode: int = 0, record: list = None):
    """Patch subprocess.run so the evidence-HEAD `git log ...` call returns pinned.

    When `record` is given, every git argv is appended (as a list) so the test
    can assert the generator really used the pathspec command. Non-git
    invocations delegate to the real subprocess.run (captured before any
    patching), so the rest of the script keeps working on the host.
    """
    def fake_run(cmd, **kwargs):
        if cmd and cmd[0] == "git":
            if record is not None:
                record.append(list(cmd))
            return _GitResult(returncode, stdout)
        return _ORIG_SUBPROCESS_RUN(cmd, **kwargs)
    return mock.patch.object(gps.subprocess, "run", side_effect=fake_run)


def git_broken():
    """Patch subprocess.run so `git ...` raises (git missing / not a repository)."""
    def fake_run(cmd, **kwargs):
        if cmd and cmd[0] == "git":
            raise FileNotFoundError(2, "No such file or directory", "git")
        return _ORIG_SUBPROCESS_RUN(cmd, **kwargs)
    return mock.patch.object(gps.subprocess, "run", side_effect=fake_run)


def run_main(argv):
    """Run gps.main() with the given argv (sys.argv patched).

    Returns (exit_code, stdout, stderr). SystemExit is the only exit path
    (main() uses sys.exit()); a bare return means success (code 0).
    """
    out, err = io.StringIO(), io.StringIO()
    with mock.patch.object(sys, "argv", ["generate_platform_status.py"] + argv):
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                gps.main()
                code = 0
            except SystemExit as exc:
                code = exc.code if isinstance(exc.code, int) else 1
    return code, out.getvalue(), err.getvalue()


class TestHeadDriftGate(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = Path(self._tmp.name)
        self.yaml = self.tmp / "platform-matrix.yaml"
        self.yaml.write_text(MINIMAL_YAML, encoding="utf-8")
        self.md = self.tmp / "platform-status.md"

    def _argv_gen(self, extra=None):
        return ["--matrix", str(self.yaml), "--output", str(self.md)] + (extra or [])

    def _argv_check(self, extra=None):
        return ["--check", "--matrix", str(self.yaml), "--output", str(self.md)] + (extra or [])

    # ------------------------------------------------------------------ S1
    def test_s1_synced_head_check_exit0(self):
        """--head <yaml value> -> no drift, md fresh -> --check exits 0."""
        code, out, err = run_main(self._argv_gen(["--head", FAKE_YAML_HEAD]))
        self.assertEqual(code, 0, f"S1 baseline generation failed: {err}")
        self.assertTrue(self.md.exists(), "baseline md was not generated")
        self.assertTrue(self.md.stat().st_size > 200, "baseline md looks like a stub")

        code, out, err = run_main(self._argv_check(["--head", FAKE_YAML_HEAD]))
        self.assertEqual(code, 0, f"S1 expected exit 0, got {code}; stderr={err!r}")
        self.assertNotIn("[WARN]", err, "no git fallback may fire when --head is given")
        self.assertNotIn("lags the code HEAD", err)
        self.assertIn("up-to-date", out)

    # ------------------------------------------------------------------ S2
    def test_s2_drift_real_git_exit1_with_fix_hint(self):
        """Real evidence HEAD differs from the yaml 8-char sha -> exit 1 + fix hint."""
        if gps._git_evidence_head() is None:
            self.skipTest("git unavailable in this environment; S2b covers here")
        # Baseline md must exist (the missing-output guard runs before drift).
        run_main(self._argv_gen(["--head", FAKE_YAML_HEAD]))
        code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 1, f"S2 expected exit 1, got {code}")
        self.assertIn("lags the code HEAD", err)
        self.assertIn(f"yaml head_commit={FAKE_YAML_HEAD}", err)
        self.assertIn("effective HEAD=", err)
        self.assertIn("(source: git)", err)
        self.assertIn("Fix:", err)
        self.assertIn("update `head_commit`", err)

    def test_s2b_drift_pinned_git_exit1_deterministic(self):
        """Pinned-evidence variant of S2: deterministic even where git is broken."""
        calls = []
        # Baseline md must exist (the missing-output guard runs before drift).
        with git_head_pinned(PINNED_GIT_HEAD, record=calls):
            run_main(self._argv_gen(["--head", FAKE_YAML_HEAD]))
        with git_head_pinned(PINNED_GIT_HEAD, record=calls):
            code, out, err = run_main(self._argv_check())
        self.assertIn(GIT_LOG_EVIDENCE_CMD, calls, f"generator must call the pathspec command, got {calls}")
        self.assertEqual(code, 1, f"S2b expected exit 1, got {code}; stderr={err!r}")
        self.assertIn("lags the code HEAD", err)
        self.assertIn(f"effective HEAD={PINNED_GIT_HEAD}", err)
        self.assertIn("Fix:", err)

    # ------------------------------------------------------------------ S3
    def test_s3_head_override_exit0_despite_differing_git(self):
        """--head <yaml value> wins even when evidence HEAD disagrees -> exit 0."""
        with git_head_pinned(PINNED_GIT_HEAD):
            code, out, err = run_main(self._argv_gen(["--head", FAKE_YAML_HEAD]))
            self.assertEqual(code, 0, f"S3 generation failed: {err}")
        with git_head_pinned(PINNED_GIT_HEAD):
            code, out, err = run_main(self._argv_check(["--head", FAKE_YAML_HEAD]))
        self.assertEqual(code, 0, f"S3 expected exit 0, got {code}; stderr={err!r}")
        self.assertIn("up-to-date", out)

    # ------------------------------------------------------------------ S4
    def test_s4_nogit_warn_fallback_and_md_compare(self):
        """git broken -> [WARN] + yaml fallback; drift skipped; md compare still runs."""
        with git_broken():
            code, out, err = run_main(self._argv_gen())
            self.assertEqual(code, 0, f"S4 generation failed: {err}")
            self.assertIn("[WARN]", err)
            self.assertIn(f"falling back to matrix yaml head_commit={FAKE_YAML_HEAD!r}", err)

        with git_broken():
            code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 0, f"S4 fresh md expected exit 0, got {code}; stderr={err!r}")
        self.assertNotIn("lags the code HEAD", err, "drift must be skipped without a usable git")

        # Tamper the md: the byte-freshness branch (not drift) must red.
        self.md.write_text(
            self.md.read_text(encoding="utf-8") + "\n<!-- tampered -->\n", encoding="utf-8"
        )
        with git_broken():
            code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 1, f"S4 tampered md expected exit 1, got {code}")
        self.assertIn("is stale or modified", err)
        self.assertNotIn("lags the code HEAD", err)

    # ------------------------------------------------------------------ S5
    def test_s5_library_call_parity_with_cli(self):
        """generate_markdown(data) without head args embeds evidence HEAD exactly as the CLI."""
        with git_head_pinned(PINNED_GIT_HEAD):
            direct = gps.generate_markdown(gps.load_yaml(self.yaml))
        self.assertIn(f"`{PINNED_GIT_HEAD}`", direct)
        self.assertIn("head_commit source: git", direct)

        with git_broken():
            direct = gps.generate_markdown(gps.load_yaml(self.yaml))
        self.assertIn(f"`{FAKE_YAML_HEAD}`", direct)
        self.assertIn("head_commit source: yaml", direct)

    # ------------------------------------------------------------------ S6
    def test_s6_json_still_uses_raw_yaml_head(self):
        """--json behavior unchanged: it reports the yaml head_commit value."""
        code, out, err = run_main(["--json", "--matrix", str(self.yaml), "--output", str(self.md)])
        self.assertEqual(code, 0, f"S6 failed: {err}")
        self.assertIn(f'"head_commit": "{FAKE_YAML_HEAD}"', out)

    def test_s7_short_sha_prefix_counts_as_synced(self):
        """A yaml head_commit that is a short prefix of the effective HEAD counts as synced."""
        yaml_short = PINNED_GIT_HEAD[:8]
        self.yaml.write_text(MINIMAL_YAML.replace(FAKE_YAML_HEAD, yaml_short), encoding="utf-8")
        with git_head_pinned(PINNED_GIT_HEAD):
            code, out, err = run_main(self._argv_gen())
            self.assertEqual(code, 0, f"S7 generation failed: {err}")
            self.assertTrue(self.md.exists(), "S7 baseline md not generated")
        with git_head_pinned(PINNED_GIT_HEAD):
            code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 0, f"S7 expected exit 0, got {code}; stderr={err!r}")
        self.assertIn("up-to-date", out)
        self.assertNotIn("lags the code HEAD", err)

    # ------------------------------------------------------------------ S8
    def test_s8_docs_only_sync_commit_does_not_drift(self):
        """Real temp repo: a docs-only sync commit is not the evidence HEAD.

        Commit 1 = code (README.md + code.txt, sha C); commit 2 = a matrix-sync
        style commit touching ONLY docs/ (sha D, the tip). With the yaml recorded
        at C, --check must be green on the docs-only tip (evidence HEAD is still
        C), and red again after a later code-only commit (evidence HEAD moved).
        """
        if gps._git_evidence_head() is None:
            self.skipTest("git unavailable in this environment")
        repo = self.tmp / "repo"
        repo.mkdir()

        def git(*args):
            return subprocess.run(
                ["git", "-C", str(repo), *args], capture_output=True, text=True
            )

        self.assertEqual(git("init", "-q").returncode, 0, "git init failed")
        for cfg in (["config", "user.email", "t@example.com"], ["config", "user.name", "t"]):
            self.assertEqual(git(*cfg).returncode, 0, "git config failed")
        (repo / "README.md").write_text("readme", encoding="utf-8")
        (repo / "code.txt").write_text("x", encoding="utf-8")
        git("add", ".")
        self.assertEqual(git("commit", "-q", "-m", "code commit").returncode, 0)
        code_sha = git("rev-parse", "HEAD").stdout.strip()

        (repo / "docs").mkdir()
        (repo / "docs" / "status.md").write_text("matrix", encoding="utf-8")
        git("add", ".")
        self.assertEqual(git("commit", "-q", "-m", "docs sync commit").returncode, 0)
        docs_tip = git("rev-parse", "HEAD").stdout.strip()
        self.assertNotEqual(docs_tip, code_sha, "docs-only commit must move the tip")

        yaml_short = code_sha[:8]
        self.yaml.write_text(MINIMAL_YAML.replace(FAKE_YAML_HEAD, yaml_short), encoding="utf-8")
        # ROOT patched to the temp repo: evidence-HEAD lookup and the evidence-
        # document existence checks both resolve inside it.
        with mock.patch.object(gps, "ROOT", repo):
            code, out, err = run_main(self._argv_gen())
            self.assertEqual(code, 0, f"S8 generation failed: {err}")
            code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 0, f"S8 docs-only sync tip must be green, got {code}; stderr={err!r}")
        self.assertNotIn("lags the code HEAD", err)

        # Now a code commit lands without a sync: evidence HEAD moves -> drift red.
        (repo / "code2.txt").write_text("y", encoding="utf-8")
        git("add", ".")
        self.assertEqual(git("commit", "-q", "-m", "code commit after sync").returncode, 0)
        new_code = git("rev-parse", "HEAD").stdout.strip()
        self.assertNotEqual(new_code[:8], yaml_short, "new code commit must differ from yaml")
        with mock.patch.object(gps, "ROOT", repo):
            code, out, err = run_main(self._argv_check())
        self.assertEqual(code, 1, f"S8 code-after-sync must drift, got {code}")
        self.assertIn("lags the code HEAD", err)

    def test_resolve_priority_cli_git_yaml(self):
        """resolve_head_commit priority: --head > evidence HEAD > yaml (direct unit check)."""
        data = {"head_commit": FAKE_YAML_HEAD}
        with git_head_pinned(PINNED_GIT_HEAD):
            eff, src = gps.resolve_head_commit("00000000", data)
            self.assertEqual((eff, src), ("00000000", "cli"))
            eff, src = gps.resolve_head_commit(None, data)
            self.assertEqual((eff, src), (PINNED_GIT_HEAD, "git"))
        with git_broken():
            eff, src = gps.resolve_head_commit(None, data)
            self.assertEqual((eff, src), (FAKE_YAML_HEAD, "yaml"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
