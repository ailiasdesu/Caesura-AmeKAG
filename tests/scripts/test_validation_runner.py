"""Behavioral tests for the validation executor; fixtures are never release evidence."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from run_validation import fingerprint_paths, run_profile


class ValidationRunnerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="caesura-validation-")
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        # Repository rules require a .git marker before invoking Git.
        (self.repo / ".git").mkdir()
        subprocess.run(["git", "init", "-q"], cwd=self.repo, check=True)
        (self.repo / ".gitignore").write_text("raw/\n", encoding="utf-8")
        (self.repo / "fixture.txt").write_text("fixture v1\n", encoding="utf-8")
        subprocess.run(["git", "add", ".gitignore", "fixture.txt"], cwd=self.repo, check=True)
        subprocess.run(
            ["git", "-c", "user.name=Validation Fixture", "-c",
             "user.email=fixture@example.invalid", "commit", "-qm", "test fixture"],
            cwd=self.repo, check=True,
        )
        self.profile_file = self.repo / "profiles.json"

    def write_profile(self, checks, **overrides):
        self.profile_file.write_text(json.dumps({
            "schema_version": 1,
            "fixture_paths": ["fixture.txt"],
            "profiles": {
                "fixture": {
                    "platform": {"win32": "windows", "darwin": "macos"}.get(sys.platform, "linux"),
                    "configuration": "Debug",
                    "fixture_paths": ["fixture.txt"],
                    "checks": checks, **overrides,
                }
            },
        }), encoding="utf-8")
        return self.profile_file

    def check(self, name="sample", code="print('hello')", **extra):
        return {
            "id": name, "parser": "exit-code", "required": True,
            "command": ["{python}", "-c", code], "cwd": "{repo}",
            "binary": "{python}", "timeout_seconds": 5, **extra,
        }

    def execute(self, checks, directory="raw/one", configuration="Debug", **profile_overrides):
        self.write_profile(checks, **profile_overrides)
        return run_profile(
            repo=self.repo, profile_file=self.profile_file, profile_name="fixture",
            build_dir=self.repo / "build", configuration=configuration,
            run_dir=self.repo / directory, purpose="test-fixture",
        )

    def test_captures_real_exit_streams_digest_and_identity(self):
        result = self.execute([self.check(code="import sys; print('PASS'); print('error', file=sys.stderr); sys.exit(7)")])
        check = result["checks"][0]
        self.assertEqual(check["exit_code"], 7)
        self.assertEqual(result["purpose"], "test-fixture")
        self.assertTrue(result["dirty"])
        self.assertEqual(len(result["source_sha"]), 40)
        self.assertEqual(len(result["worktree_fingerprint"]), 64)
        for field, expected in [("stdout", b"PASS"), ("stderr", b"error")]:
            ref = check[field]
            data = (self.repo / "raw/one" / ref["path"]).read_bytes()
            self.assertIn(expected, data)
            self.assertEqual(hashlib.sha256(data).hexdigest(), ref["sha256"])
        self.assertLessEqual(check["started_at"], check["finished_at"])
        self.assertEqual(check["binary"]["sha256"], hashlib.sha256(Path(sys.executable).read_bytes()).hexdigest())
        saved = json.loads((self.repo / "raw/one/run.json").read_text(encoding="utf-8"))
        self.assertEqual(saved, result)

    def test_failed_check_does_not_hide_later_required_checks(self):
        result = self.execute([
            self.check("first", "raise SystemExit(3)"),
            self.check("second", "print('second really ran')"),
        ])
        self.assertEqual([c["exit_code"] for c in result["checks"]], [3, 0])

    def test_timeout_is_recorded_as_failure_with_partial_output(self):
        # The process manager has real timeout/descendant integration tests.
        # Inject its boundary here so this assertion is independent of Python startup speed.
        def timeout(argv, cwd, stdout, stderr, seconds):
            stdout.write(b"began\n")
            stdout.flush()
            raise subprocess.TimeoutExpired(argv, seconds)
        with patch("run_validation.run_owned_command", side_effect=timeout):
            result = self.execute([self.check(timeout_seconds=0.1)])
        check = result["checks"][0]
        self.assertNotEqual(check["exit_code"], 0)
        self.assertEqual(check["error"], "timeout")
        self.assertIn("began", (self.repo / "raw/one" / check["stdout"]["path"]).read_text())

    def test_refuses_to_overwrite_existing_run(self):
        self.execute([self.check()])
        previous = (self.repo / "raw/one/run.json").read_bytes()
        with self.assertRaises(FileExistsError):
            self.execute([self.check(code="print('replacement')")])
        self.assertEqual((self.repo / "raw/one/run.json").read_bytes(), previous)

    def test_arguments_are_not_interpreted_as_shell_code(self):
        marker = self.repo / "injected.txt"
        payload = f"literal & echo injected > {marker}"
        result = self.execute([self.check(
            command=["{python}", "-c", "import sys; print(sys.argv[1])", payload],
        )])
        self.assertEqual(result["checks"][0]["exit_code"], 0)
        self.assertFalse(marker.exists())
        output = self.repo / "raw/one" / result["checks"][0]["stdout"]["path"]
        self.assertIn(payload, output.read_text(encoding="utf-8"))

    def test_missing_required_binary_is_recorded_and_not_run(self):
        result = self.execute([self.check(binary="{repo}/missing-program.exe")])
        self.assertNotEqual(result["checks"][0]["exit_code"], 0)
        self.assertEqual(result["checks"][0]["error"], "missing_binary")

    def test_duplicate_check_ids_and_unknown_variables_are_rejected(self):
        for checks in [
            [self.check("same"), self.check("same")],
            [self.check(command=["{unknown_program}"])],
        ]:
            with self.subTest(checks=checks), self.assertRaises(ValueError):
                self.execute(checks)

    def test_fixture_fingerprint_covers_names_content_and_missing_files(self):
        before = fingerprint_paths(self.repo, ["fixture.txt"])
        (self.repo / "fixture.txt").write_text("fixture v2\n", encoding="utf-8")
        self.assertNotEqual(before, fingerprint_paths(self.repo, ["fixture.txt"]))
        with self.assertRaises(FileNotFoundError):
            fingerprint_paths(self.repo, ["missing.fixture"])

    def fixture_root_aliases(self):
        root = self.repo / "fixture-root"
        root.mkdir()
        (root / "fixture.txt").write_text("fixture alias contents\n", encoding="utf-8")
        (root / "alias-segment").mkdir()
        # Parent traversal is a real root alias on every supported host and
        # needs no Windows symlink privilege. POSIX also tests a root symlink,
        # matching the macOS /var -> /private/var temporary-directory case.
        aliases = [root / "alias-segment" / ".."]
        if sys.platform != "win32":
            linked = self.repo / "fixture-root-link"
            linked.symlink_to(root, target_is_directory=True)
            aliases.append(linked)
        return root, aliases

    def test_fixture_fingerprint_accepts_equivalent_root_aliases(self):
        root, aliases = self.fixture_root_aliases()
        expected = fingerprint_paths(root.resolve(), ["fixture.txt"])
        for alias in aliases:
            with self.subTest(alias=alias):
                self.assertNotEqual(alias, alias.resolve())
                self.assertEqual(alias.resolve(), root.resolve())
                self.assertEqual(fingerprint_paths(alias, ["fixture.txt"]), expected)

    def test_fixture_fingerprint_alias_root_still_rejects_outside_targets(self):
        root, aliases = self.fixture_root_aliases()
        outside = self.repo / "outside.fixture"
        outside.write_text("outside the fixture root\n", encoding="utf-8")
        candidates = ["../outside.fixture", str(outside.resolve())]
        if sys.platform != "win32":
            (root / "outside-link.fixture").symlink_to(outside)
            candidates.append("outside-link.fixture")
        for alias in aliases:
            for candidate in candidates:
                with self.subTest(alias=alias, candidate=candidate):
                    with self.assertRaisesRegex(ValueError, "Fixture escapes repository"):
                        fingerprint_paths(alias, [candidate])

    def test_marks_source_changes_during_execution(self):
        code = "from pathlib import Path; Path('fixture.txt').write_text('changed during test')"
        result = self.execute([self.check(code=code)])
        self.assertTrue(result["source_changed_during_run"])
        self.assertNotEqual(result["worktree_fingerprint"], result["finished_worktree_fingerprint"])

    def write_cache(self, source=None, configuration="Debug"):
        build = self.repo / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={source or self.repo}\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja\n"
            f"CMAKE_BUILD_TYPE:STRING={configuration}\n", encoding="utf-8",
        )

    def test_rejects_foreign_host_and_configuration(self):
        with self.assertRaises(ValueError):
            self.execute([self.check()], platform="another-platform")
        with self.assertRaises(ValueError):
            self.execute([self.check()], configuration="Release")

    def test_rejects_build_directory_from_another_checkout(self):
        self.write_cache(source=self.repo / "another-checkout")
        with self.assertRaises(ValueError):
            self.execute([self.check()], require_cmake_cache=True)

    def test_rejects_debug_single_config_build_labeled_release(self):
        self.write_cache(configuration="Debug")
        self.write_profile([self.check()], configuration="Release", require_cmake_cache=True)
        with self.assertRaises(ValueError):
            run_profile(repo=self.repo, profile_file=self.profile_file, profile_name="fixture",
                        build_dir=self.repo / "build", configuration="Release",
                        run_dir=self.repo / "raw/one", purpose="test-fixture")

    def test_rejects_build_without_required_validation_prerequisites(self):
        self.write_cache()
        with self.assertRaises(ValueError):
            self.execute([self.check()], require_cmake_cache=True,
                         expected_cache={"CAESURA_REQUIRE_TEST_PREREQUISITES": "ON"})

    def test_binary_replacement_cannot_relabel_the_executed_bytes(self):
        binary = self.repo / "binary.fixture"
        binary.write_bytes(b"before")
        expected = hashlib.sha256(binary.read_bytes()).hexdigest()
        result = self.execute([self.check(
            code="from pathlib import Path; Path('binary.fixture').write_bytes(b'after')",
            binary="{repo}/binary.fixture",
        )])
        check = result["checks"][0]
        self.assertEqual(check["binary"]["sha256"], expected)
        self.assertEqual(check["error"], "binary_changed")
        self.assertNotEqual(check["exit_code"], 0)

    def test_ignored_fixture_changes_are_reported_separately(self):
        ignored = self.repo / "ignored"
        ignored.mkdir()
        (ignored / "fixture.txt").write_text("before", encoding="utf-8")
        (self.repo / ".gitignore").write_text("raw/\nignored/\n", encoding="utf-8")
        result = self.execute([self.check(
            code="from pathlib import Path; Path('ignored/fixture.txt').write_text('after')",
        )], fixture_paths=["ignored"])
        self.assertTrue(result["fixtures_changed_during_run"])
        self.assertFalse(result["source_changed_during_run"])


if __name__ == "__main__":
    unittest.main()
