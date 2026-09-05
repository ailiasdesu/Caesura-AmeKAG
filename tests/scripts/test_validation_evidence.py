"""Validation evidence contracts. All fabricated inputs stay in test-fixture sandboxes."""
from __future__ import annotations

import copy
import contextlib
import hashlib
import io
import json
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
sys.path.insert(0, str(SCRIPTS))

from collect_validation_evidence import EvidenceError, collect_evidence, parse_report
from verify_release_candidate import verify_evidence


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class EvidenceFixture:
    """Explicit test data; even an internally valid fixture cannot pass release verification."""

    def __init__(self, root: Path):
        self.root = root
        self.raw = root / "raw"
        self.raw.mkdir()
        self.profile_path = root / "profiles.json"
        self.run_path = self.raw / "run.json"
        self.profile = {
            "schema_version": 1,
            "profiles": {"test-debug": {
                "platform": "test", "configuration": "Debug",
                "checks": [{"id": "cpp", "parser": "doctest", "required": True,
                            "min_discovered": 2, "allowed_skips": []}],
            }},
        }
        self.write_profile()
        self.run = {
            "schema_version": 1,
            "source_sha": "a" * 40, "dirty": False,
            "worktree_fingerprint": "b" * 64, "fixture_sha256": "c" * 64,
            "finished_worktree_fingerprint": "b" * 64, "source_changed_during_run": False,
            "finished_fixture_sha256": "c" * 64, "fixtures_changed_during_run": False,
            "profile_name": "test-debug",
            "run_id": "fixture-001", "run_attempt": 1,
            "repository": "test/fixture", "workflow": "fixture-workflow",
            "platform": "test", "configuration": "Debug",
            "started_at": "2026-09-05T01:00:00Z", "finished_at": "2026-09-05T01:00:02Z",
            "toolchain": {"python": sys.version.split()[0]}, "profile_variables": {},
            "profile_sha256": sha(self.profile_path), "purpose": "test-fixture",
            "checks": [{
                "id": "cpp", "command": ["fixture-runner", "--counted"], "cwd": str(root),
                "started_at": "2026-09-05T01:00:00Z", "finished_at": "2026-09-05T01:00:01Z",
                "exit_code": 0,
                "stdout": self.file("cpp.stdout", self.doctest()),
                "stderr": self.file("cpp.stderr", ""),
                "binary": self.file("runner.bin", "EXPLICIT TEST FIXTURE; NOT A RELEASE BINARY"),
                "executed_program": self.file("program.bin", "EXPLICIT TEST EXECUTABLE FIXTURE"),
            }],
        }
        self.output = root / "validation" / self.run["source_sha"] / self.run["run_id"] / "test-debug"
        self.write_run()

    @staticmethod
    def doctest(passed: int = 2, failed: int = 0, skipped: int = 0) -> str:
        total = passed + failed
        return (f"[doctest] test cases: {total} | {passed} passed | {failed} failed | {skipped} skipped\n"
                "[doctest] assertions: 4 | 4 passed | 0 failed |\n")

    def file(self, name: str, text: str) -> dict:
        path = self.raw / name
        path.write_text(text, encoding="utf-8")
        return {"path": name, "sha256": sha(path)}

    def write_profile(self):
        self.profile_path.write_text(json.dumps(self.profile), encoding="utf-8")

    def write_run(self):
        self.run_path.write_text(json.dumps(self.run), encoding="utf-8")

    def collect(self):
        self.write_run()
        return collect_evidence(self.profile_path, "test-debug", self.run_path, self.output)

    def verify(self, release=False):
        return verify_evidence(self.output, self.profile_path, "test-debug", self.run_path,
                               source_sha="a" * 40, release=release)


class ValidationEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="caesura-evidence-test-")
        self.addCleanup(self.tmp.cleanup)
        self.f = EvidenceFixture(Path(self.tmp.name))

    def test_complete_fixture_can_be_diagnosed_but_never_released(self):
        result = self.f.collect()
        self.assertEqual(result["result"], "PASS")
        self.assertEqual(result["checks"][0]["counts"]["passed"], 2)
        self.assertEqual(self.f.verify(), [])
        self.assertTrue(any("test-fixture" in error for error in self.f.verify(release=True)))

    def test_fake_pass_with_nonzero_exit_is_failed(self):
        self.f.run["checks"][0]["exit_code"] = 1
        self.assertEqual(self.f.collect()["result"], "FAIL")
        self.assertTrue(self.f.verify())

    def test_failed_test_cannot_be_hidden_by_zero_exit(self):
        self.f.run["checks"][0]["stdout"] = self.f.file("cpp.stdout", self.f.doctest(1, 1))
        self.assertEqual(self.f.collect()["result"], "FAIL")

    def test_zero_tests_rejected(self):
        self.f.run["checks"][0]["stdout"] = self.f.file("cpp.stdout", self.f.doctest(0))
        self.assertEqual(self.f.collect()["result"], "FAIL")

    def test_missing_summary_is_not_a_pass(self):
        self.f.run["checks"][0]["stdout"] = self.f.file("cpp.stdout", "PASS\n")
        self.assertEqual(self.f.collect()["result"], "FAIL")

    def test_missing_stdout_is_input_error(self):
        (self.f.raw / "cpp.stdout").unlink()
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_empty_stderr_is_preserved(self):
        result = self.f.collect()
        path = self.f.output / result["checks"][0]["files"]["stderr"]["path"]
        self.assertEqual(path.read_bytes(), b"")

    def test_digest_mismatch_rejected_at_collection(self):
        self.f.run["checks"][0]["binary"]["sha256"] = "d" * 64
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_executed_program_is_collected_and_verified(self):
        manifest = self.f.collect()
        reference = manifest["checks"][0]["files"]["executed_program"]
        program = self.f.output / reference["path"]
        self.assertEqual(sha(program), reference["sha256"])
        program.write_text("changed executable", encoding="utf-8")
        self.assertTrue(self.f.verify())

    def test_success_without_executed_program_is_rejected(self):
        del self.f.run["checks"][0]["executed_program"]
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_executed_program_digest_mismatch_is_rejected(self):
        self.f.run["checks"][0]["executed_program"]["sha256"] = "d" * 64
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_report_bytes_are_authenticated_before_they_are_parsed(self):
        from collect_validation_evidence import build_manifest
        log = self.f.raw / "cpp.stdout"
        original_open = Path.open
        reads = []

        def replace_on_second_read(path, mode="r", *args, **kwargs):
            if path == log and "r" in mode:
                reads.append(mode)
                if len(reads) == 2:
                    with original_open(path, "wb") as stream:
                        stream.write(self.f.doctest(9000).encode("utf-8"))
            return original_open(path, mode, *args, **kwargs)

        with patch.object(Path, "open", replace_on_second_read):
            manifest, _ = build_manifest(self.f.profile_path, "test-debug", self.f.run_path)
        self.assertEqual(manifest["checks"][0]["counts"]["passed"], 2)
        self.assertEqual(len(reads), 1)

    def test_report_read_is_bounded(self):
        self.f.run["checks"][0]["stdout"] = self.f.file("cpp.stdout", "x" * 8193)
        with patch("collect_validation_evidence.MAX_REPORT_BYTES", 8192):
            with self.assertRaises(EvidenceError):
                self.f.collect()

    def test_expected_unittest_failure_fails_required_gate(self):
        self.f.profile["profiles"]["test-debug"]["checks"][0]["parser"] = "unittest"
        self.f.write_profile()
        self.f.run["profile_sha256"] = sha(self.f.profile_path)
        self.f.run["checks"][0]["stdout"] = self.f.file(
            "cpp.stdout", "Ran 2 tests in 0.03s\n\nOK (expected failures=1)\n")
        manifest = self.f.collect()
        self.assertEqual(manifest["result"], "FAIL")
        self.assertEqual(manifest["checks"][0]["counts"]["passed"], 1)
        self.assertTrue(self.f.verify())

    def test_duplicate_run_does_not_overwrite_first(self):
        self.f.collect()
        before = (self.f.output / "manifest.json").read_bytes()
        with self.assertRaises(EvidenceError):
            self.f.collect()
        self.assertEqual((self.f.output / "manifest.json").read_bytes(), before)

    def test_unknown_check_rejected(self):
        self.f.run["checks"].append({**self.f.run["checks"][0], "id": "unknown"})
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_duplicate_check_rejected(self):
        self.f.run["checks"].append(copy.deepcopy(self.f.run["checks"][0]))
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_missing_required_check_is_not_run(self):
        self.f.run["checks"] = []
        result = self.f.collect()
        self.assertEqual(result["result"], "FAIL")
        self.assertEqual(result["checks"][0]["result"], "NOT_RUN")

    def test_wrong_profile_digest_rejected(self):
        self.f.run["profile_sha256"] = "f" * 64
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_profile_commands_bound_to_executed_argv(self):
        check = self.f.profile["profiles"]["test-debug"]["checks"][0]
        check["command"] = ["{repo}/runner", "--all"]
        self.f.write_profile()
        self.f.run["profile_sha256"] = sha(self.f.profile_path)
        self.f.run["profile_variables"] = {"repo": str(self.f.root)}
        with self.assertRaises(EvidenceError):
            self.f.collect()

    def test_wrong_workflow_receipt_rejected_after_collection(self):
        self.f.collect()
        self.f.run["workflow"] = "untrusted-workflow"
        self.f.write_run()
        self.assertTrue(self.f.verify())

    def test_wrong_run_attempt_receipt_rejected_after_collection(self):
        self.f.collect()
        self.f.run["run_attempt"] = 2
        self.f.write_run()
        self.assertTrue(self.f.verify())

    def test_wrong_source_receipt_rejected_after_collection(self):
        self.f.collect()
        self.f.run["source_sha"] = "d" * 40
        self.f.write_run()
        self.assertTrue(self.f.verify())

    def test_tampered_manifest_counts_rejected(self):
        manifest = self.f.collect()
        manifest["checks"][0]["counts"]["passed"] = 9000
        (self.f.output / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        self.assertTrue(self.f.verify())

    def test_tampered_log_rejected(self):
        manifest = self.f.collect()
        log = self.f.output / manifest["checks"][0]["files"]["stdout"]["path"]
        log.write_text(self.f.doctest(9000), encoding="utf-8")
        self.assertTrue(self.f.verify())

    def test_dirty_validation_can_be_diagnosed_but_not_released(self):
        self.f.run["purpose"] = "validation"
        self.f.run["dirty"] = True
        self.f.collect()
        self.assertEqual(self.f.verify(), [])
        self.assertTrue(any("dirty" in error for error in self.f.verify(release=True)))

    def test_cli_collects_then_verifies_without_auto_go(self):
        result = subprocess.run([
            sys.executable, str(SCRIPTS / "verify_release_candidate.py"), "--generate-bundle",
            "--profile", str(self.f.profile_path), "--profile-name", "test-debug",
            "--run", str(self.f.run_path), "--expected-run", str(self.f.run_path),
            "--artifacts-dir", str(self.f.output), "--commit", "a" * 40, "--diagnostic",
        ], capture_output=True, text=True, encoding="utf-8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("RC-GO", result.stdout)
        self.assertTrue((self.f.output / "manifest.json").exists())

    def test_real_executor_collect_verify_roundtrip_in_fixture_sandbox(self):
        from run_validation import run_profile
        child = self.f.root / "child_test.py"
        child.write_text("import unittest\nclass Check(unittest.TestCase):\n def test_real(self): self.assertEqual(2+2,4)\nunittest.main()\n", encoding="utf-8")
        profile = self.f.profile["profiles"]["test-debug"]
        profile["platform"] = {"Windows": "windows", "Linux": "linux", "Darwin": "macos"}[platform.system()]
        profile["fixture_paths"] = ["child_test.py"]
        profile["checks"] = [{"id": "child", "parser": "unittest", "required": True,
                               "min_discovered": 1, "allowed_skips": [],
                               "command": ["{python}", "{repo}/child_test.py"],
                               "cwd": "{repo}", "binary": "{python}"}]
        self.f.write_profile()
        identity = {"source_sha": "a" * 40, "dirty": False, "worktree_fingerprint": "b" * 64}
        raw = self.f.root / "actual-execution"
        with patch("run_validation._source_identity", return_value=identity):
            receipt = run_profile(repo=self.f.root, profile_file=self.f.profile_path,
                                  profile_name="test-debug", build_dir=self.f.root / "build",
                                  configuration="Debug", run_dir=raw, purpose="test-fixture")
        output = self.f.root / "collected" / receipt["source_sha"] / receipt["run_id"] / "test-debug"
        manifest = collect_evidence(self.f.profile_path, "test-debug", raw / "run.json", output)
        self.assertEqual(manifest["checks"][0]["counts"]["passed"], 1)
        self.assertEqual(verify_evidence(output, self.f.profile_path, "test-debug", raw / "run.json",
                                         source_sha="a" * 40, release=False), [])
        self.assertTrue(verify_evidence(output, self.f.profile_path, "test-debug", raw / "run.json",
                                       source_sha="a" * 40, release=True))

    def test_profile_binary_binding_is_portable_between_verifier_hosts(self):
        from collect_validation_evidence import validate_check
        spec = {"id": "cpp", "binary": "{repo}/build/Debug/tests.exe", "cwd": "{repo}"}
        run = {**self.f.run, "profile_variables": {"repo": "/opt/ci/source"}}
        check = {**self.f.run["checks"][0], "cwd": "/opt/ci/source",
                 "binary": {"path": "/opt/ci/source/build/Debug/tests.exe", "sha256": "a" * 64}}
        validate_check(check, spec, run)

    def test_exit_code_check_can_have_empty_output(self):
        self.f.profile["profiles"]["test-debug"]["checks"][0].update(parser="exit-code", min_discovered=0)
        self.f.write_profile()
        self.f.run["profile_sha256"] = sha(self.f.profile_path)
        self.f.run["checks"][0]["stdout"] = self.f.file("cpp.stdout", "")
        manifest = self.f.collect()
        self.assertEqual(manifest["result"], "PASS")
        self.assertIsNone(manifest["checks"][0]["counts"])


class ReportParserTests(unittest.TestCase):
    def parse(self, parser, text, report=None):
        return parse_report(parser, text, "", report)

    def test_doctest_discovery_includes_filtered_cases(self):
        counts, skipped = self.parse("doctest", EvidenceFixture.doctest(2, 0, 5))
        self.assertEqual(counts, {"discovered": 7, "executed": 2, "passed": 2, "failed": 0, "skipped": 5})

    def test_lua_uses_final_runner_summary(self):
        counts, _ = self.parse("lua", "Results: 123 passed, 0 failed\nResults: 3 passed, 0 failed, 3 total\n")
        self.assertEqual(counts["passed"], 3)

    def test_lua_inconsistent_summary_rejected(self):
        with self.assertRaises(EvidenceError):
            self.parse("lua", "Results: 2 passed, 1 failed, 2 total\n")

    def test_unittest_report_on_stderr_supported(self):
        counts, _ = parse_report("unittest", "", "Ran 5 tests in 0.03s\n\nOK\n", None)
        self.assertEqual(counts["passed"], 5)

    def test_unittest_failures_are_counted(self):
        counts, _ = self.parse("unittest", "Ran 5 tests in 0.03s\n\nFAILED (failures=1, errors=1)\n")
        self.assertEqual(counts["failed"], 2)

    def test_unittest_expected_failures_are_not_passes(self):
        counts, _ = self.parse("unittest", "Ran 5 tests in 0.03s\n\nOK (expected failures=2)\n")
        self.assertEqual(counts, {"discovered": 5, "executed": 5, "passed": 3, "failed": 2, "skipped": 0})

    def test_ctest_skips_have_names(self):
        xml = '<testsuite tests="2" failures="0" disabled="1" skipped="0"><testcase name="core" status="run"/><testcase name="ai" status="disabled"/></testsuite>'
        counts, skipped = self.parse("ctest-junit", "", xml)
        self.assertEqual(counts["skipped"], 1)
        self.assertEqual(skipped, ["ai"])

    def test_ctest_malformed_xml_rejected(self):
        with self.assertRaises(EvidenceError):
            self.parse("ctest-junit", "", "<testsuite>")

    def test_ctest_failure_summary_cannot_hide_failure(self):
        with self.assertRaises(EvidenceError):
            self.parse("ctest-junit", "", '<testsuite tests="1" failures="1"><testcase name="core"/></testsuite>')

    def test_ctest_skip_and_disabled_summary_must_match_cases(self):
        for attribute in ("skipped", "disabled"):
            skipped_case = '<testcase name="core"><skipped/></testcase>' if attribute == "skipped" else '<testcase name="core" status="disabled"/>'
            for declared, node in (("1", '<testcase name="core"/>'), ("0", skipped_case), ("invalid", '<testcase name="core"/>')):
                with self.subTest(attribute=attribute, declared=declared, node=node):
                    with self.assertRaises(EvidenceError):
                        self.parse("ctest-junit", "", f'<testsuite tests="1" {attribute}="{declared}">{node}</testsuite>')

    def test_ctest_disabled_and_runtime_skip_are_distinct_summary_categories(self):
        # Matches CTest --output-junit with DISABLED and SKIP_RETURN_CODE cases.
        xml = ('<testsuite tests="3" failures="0" disabled="1" skipped="1">'
               '<testcase name="pass" status="run"/>'
               '<testcase name="skip" status="notrun"><skipped message="SKIP_RETURN_CODE=77"/></testcase>'
               '<testcase name="disabled" status="disabled"/></testsuite>')
        counts, skipped = self.parse("ctest-junit", "", xml)
        self.assertEqual(counts, {"discovered": 3, "executed": 1, "passed": 1, "failed": 0, "skipped": 2})
        self.assertEqual(skipped, ["skip", "disabled"])

    def test_ctest_nested_suite_summaries_must_match_cases(self):
        xml = '<testsuites tests="1"><testsuite tests="1" skipped="1"><testcase name="core"/></testsuite></testsuites>'
        with self.assertRaises(EvidenceError):
            self.parse("ctest-junit", "", xml)

    def test_vitest_results_use_actual_assertions(self):
        report = json.dumps({"numTotalTests": 2, "numPassedTests": 1, "numFailedTests": 0,
                             "numPendingTests": 1, "testResults": [{"assertionResults": [
                                 {"fullName": "works", "status": "passed"},
                                 {"fullName": "asset", "status": "pending"}]}]})
        counts, skipped = self.parse("vitest-json", "", report)
        self.assertEqual(counts["passed"], 1)
        self.assertEqual(skipped, ["asset"])

    def test_vitest_summary_cannot_contradict_assertions(self):
        with self.assertRaises(EvidenceError):
            self.parse("vitest-json", "", json.dumps({"numTotalTests": 50, "testResults": []}))

    def test_vitest_failed_run_cannot_hide_behind_passed_assertions(self):
        report = {"success": False, "numTotalTests": 1, "numPassedTests": 1, "numFailedTests": 0,
                  "numPendingTests": 0, "testResults": [{"assertionResults": [{"fullName": "core", "status": "passed"}]}]}
        with self.assertRaises(EvidenceError):
            self.parse("vitest-json", "", json.dumps(report))


class EvidenceCliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="caesura-evidence-cli-")
        self.addCleanup(temporary.cleanup)
        self.f = EvidenceFixture(Path(temporary.name))

    def invoke(self, main, args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(sys, "argv", ["evidence-cli", *map(str, args)]), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = main()
        return code, stdout.getvalue() + stderr.getvalue()

    def collect_args(self):
        return ["--profile", self.f.profile_path, "--profile-name", "test-debug", "--run", self.f.run_path, "--output", self.f.output]

    def verify_args(self):
        return ["--profile", self.f.profile_path, "--profile-name", "test-debug", "--expected-run", self.f.run_path,
                "--artifacts-dir", self.f.output, "--commit", "a" * 40]

    def test_collector_exit_codes_distinguish_failure_and_success(self):
        from collect_validation_evidence import main
        code, _ = self.invoke(main, self.collect_args())
        self.assertEqual(code, 0)
        self.f.run["run_id"] = "failed-run"
        self.f.output = self.f.output.parent.parent / "failed-run" / "test-debug"
        self.f.run["checks"][0]["exit_code"] = 2
        self.f.write_run()
        code, _ = self.invoke(main, self.collect_args())
        self.assertEqual(code, 1)

    def test_collector_missing_input_returns_failure(self):
        from collect_validation_evidence import main
        (self.f.raw / "cpp.stdout").unlink()
        code, output = self.invoke(main, self.collect_args())
        self.assertEqual(code, 1)
        self.assertIn("Missing stdout", output)

    def test_verifier_optional_missing_is_skip_not_success(self):
        from verify_release_candidate import main
        code, output = self.invoke(main, ["--artifacts-dir", self.f.output, "--skip-if-missing"])
        self.assertEqual(code, 77)
        self.assertIn("SKIP", output)

    def test_verifier_requires_external_profile_and_receipt(self):
        from verify_release_candidate import main
        code, _ = self.invoke(main, ["--artifacts-dir", self.f.output])
        self.assertEqual(code, 1)

    def test_generate_from_receipt_and_release_refusal_leave_fixture_unchanged(self):
        from verify_release_candidate import main
        code, output = self.invoke(main, [*self.verify_args(), "--generate-bundle", "--run", self.f.run_path, "--diagnostic"])
        self.assertEqual(code, 0, output)
        before = (self.f.output / "manifest.json").read_bytes()
        code, output = self.invoke(main, self.verify_args())
        self.assertEqual(code, 1)
        self.assertIn("test-fixture", output)
        self.assertEqual((self.f.output / "manifest.json").read_bytes(), before)

    def test_legacy_manual_report_cannot_replace_receipt(self):
        from verify_release_candidate import main
        code, output = self.invoke(main, [*self.verify_args(), "--report-file", self.f.root / "legacy.md"])
        self.assertEqual(code, 1)
        self.assertIn("Legacy", output)

    def test_missing_git_never_returns_a_historic_commit(self):
        from verify_release_candidate import get_target_commit
        with self.assertRaises(EvidenceError):
            get_target_commit(self.f.root)
        with self.assertRaises(EvidenceError):
            get_target_commit(self.f.root, "a59bab9")

    def test_generate_requires_actual_receipt_argument(self):
        from verify_release_candidate import main
        code, output = self.invoke(main, [*self.verify_args(), "--generate-bundle", "--diagnostic"])
        self.assertEqual(code, 1)
        self.assertIn("--run", output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
