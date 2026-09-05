"""Exercise the real CMake fixture mirror only in disposable directories."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


HELPER = Path(__file__).resolve().parents[1] / "SyncTestAssets.cmake"
FIXTURES = ("scripts", "assets", "demo", "tests/audio")


class FixtureSyncTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="caesura-fixture-sync-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / "source"
        self.build = self.source / "build"
        self.test_output = self.build / "tests" / "Debug"
        self.app_output = self.build / "Debug"
        for relative in FIXTURES:
            self.write(self.source / relative / "current.txt", relative)
        for output in (self.test_output, self.app_output):
            self.write(output / "scripts" / "existing.txt", "untouched until validated")
            self.write(output / "saves" / "slot1.json", "save")
            self.write(output / "engine.exe", "executable")
            self.write(output / "runtime.dll", "library")
            self.write(output / "tests" / "unrelated.txt", "unrelated")

    @staticmethod
    def write(path: Path, content: str):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def invoke(self, **overrides):
        values = {
            "SOURCE_ROOT": self.source,
            "BUILD_ROOT": self.build,
            "TEST_OUTPUT": self.test_output,
            "APP_OUTPUT": self.app_output,
        } | overrides
        return subprocess.run(
            ["cmake", *(f"-DCAESURA_FIXTURE_{key}={value}" for key, value in values.items()),
             "-P", str(HELPER)],
            capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30,
        )

    def assert_rejected_without_mutation(self, message: str, **overrides):
        result = self.invoke(**overrides)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Fixture sync:", result.stderr)
        self.assertIn(message, result.stderr)
        for output in (self.test_output, self.app_output):
            self.assertEqual((output / "scripts" / "existing.txt").read_text(),
                             "untouched until validated")
            self.assertEqual((output / "engine.exe").read_text(), "executable")

    def redirect(self, link: Path, target: Path):
        link.parent.mkdir(parents=True, exist_ok=True)
        target.mkdir(parents=True, exist_ok=True)
        if os.name == "nt":
            # Junction creation works without Developer Mode or elevated symlink rights.
            quote = lambda value: "'" + str(value).replace("'", "''") + "'"
            result = subprocess.run(
                ["powershell", "-NoProfile", "-Command",
                 f"New-Item -ItemType Junction -Path {quote(link)} -Target {quote(target)} | Out-Null"],
                capture_output=True, text=True, timeout=15,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.addCleanup(link.rmdir)
        else:
            link.symlink_to(target, target_is_directory=True)
            self.addCleanup(link.unlink)

    def test_mirror_updates_and_removes_deleted_fixtures_preserving_other_outputs(self):
        for relative in FIXTURES:
            self.write(self.source / relative / "deleted.txt", "removed in next revision")
        first = self.invoke()
        self.assertEqual(first.returncode, 0, first.stderr)
        for relative in FIXTURES:
            self.write(self.source / relative / "current.txt", "changed fixture")
            (self.source / relative / "deleted.txt").unlink()
            self.write(self.source / relative / "new" / "updated.txt", "new fixture")
        second = self.invoke()
        self.assertEqual(second.returncode, 0, second.stderr)
        for output, fixtures in ((self.test_output, FIXTURES),
                                 (self.app_output, FIXTURES[:3])):
            for relative in fixtures:
                self.assertFalse((output / relative / "deleted.txt").exists())
                self.assertEqual((output / relative / "current.txt").read_text(), "changed fixture")
                self.assertEqual((output / relative / "new" / "updated.txt").read_text(),
                                 "new fixture")
            self.assertFalse((output / "scripts" / "existing.txt").exists())
            for relative, content in (("saves/slot1.json", "save"), ("engine.exe", "executable"),
                                      ("runtime.dll", "library"), ("tests/unrelated.txt", "unrelated")):
                self.assertEqual((output / relative).read_text(), content)
        self.assertFalse((self.app_output / "tests" / "audio").exists())

    def test_missing_last_source_is_rejected_before_any_output_changes(self):
        shutil.rmtree(self.source / "tests" / "audio")
        self.assert_rejected_without_mutation("missing source fixture")

    def test_relative_root_is_rejected(self):
        self.assert_rejected_without_mutation("must be absolute", SOURCE_ROOT="source")

    def test_output_outside_build_is_rejected(self):
        self.assert_rejected_without_mutation("outside build root", APP_OUTPUT=self.root / "elsewhere")

    def test_sibling_prefix_is_not_inside_build(self):
        self.assert_rejected_without_mutation("outside build root", APP_OUTPUT=Path(str(self.build) + "-other"))

    def test_traversal_outside_build_is_rejected(self):
        self.assert_rejected_without_mutation("outside build root", APP_OUTPUT=self.build / ".." / "elsewhere")

    def test_file_in_last_destination_parent_is_rejected_before_mutation(self):
        self.write(self.app_output / "assets", "not a directory")
        self.assert_rejected_without_mutation("expected directory")

    def test_filesystem_root_cannot_be_build_root(self):
        self.assert_rejected_without_mutation("filesystem root", BUILD_ROOT=Path(self.root.anchor))

    def test_build_root_cannot_equal_or_contain_source(self):
        for build in (self.source, self.root):
            with self.subTest(build=build):
                self.assert_rejected_without_mutation("source root or its ancestor", BUILD_ROOT=build)

    def test_build_and_output_cannot_overlap_source_fixture(self):
        self.assert_rejected_without_mutation(
            "overlaps source fixture", BUILD_ROOT=self.source / "assets",
            TEST_OUTPUT=self.source / "assets" / "tests-output",
            APP_OUTPUT=self.source / "assets" / "app-output",
        )

    def test_nested_output_inside_owned_tree_is_rejected(self):
        self.assert_rejected_without_mutation("overlapping destination", APP_OUTPUT=self.test_output / "assets")

    def test_shared_output_directory_is_supported(self):
        result = self.invoke(APP_OUTPUT=self.test_output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.test_output / "tests" / "audio" / "current.txt").is_file())

    def test_nonexistent_output_beneath_redirected_parent_is_rejected(self):
        outside = self.root / "outside"
        self.write(outside / "sentinel.txt", "outside unchanged")
        self.redirect(self.build / "redirect", outside)
        self.assert_rejected_without_mutation("redirect", APP_OUTPUT=self.build / "redirect" / "new-output")
        self.assertEqual((outside / "sentinel.txt").read_text(), "outside unchanged")
        self.assertFalse((outside / "new-output").exists())

    def test_redirected_source_descendant_is_rejected(self):
        self.redirect(self.source / "assets" / "redirect", self.root / "external-source")
        self.assert_rejected_without_mutation("redirect")

    def test_redirected_destination_descendant_is_rejected(self):
        outside = self.root / "outside"
        self.write(outside / "sentinel.txt", "outside unchanged")
        self.redirect(self.app_output / "scripts" / "redirect", outside)
        self.assert_rejected_without_mutation("redirect")
        self.assertEqual((outside / "sentinel.txt").read_text(), "outside unchanged")

    def test_literal_bracket_directories_are_mirrored(self):
        names = ("[literal]", "nested[part]extra")
        for name in names:
            self.write(self.source / "assets" / name / "content.txt", name)
        bracket_output = self.build / "[application]"
        result = self.invoke(APP_OUTPUT=bracket_output)
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in names:
            for output in (self.test_output, bracket_output):
                self.assertEqual((output / "assets" / name / "content.txt").read_text(), name)

    def assert_named_redirect_rejected(self, name: str, *, destination: bool = False):
        base = (self.app_output if destination else self.source) / "assets"
        # A semicolon split can begin with an otherwise valid existing sibling.
        self.write(base / "sibling" / "normal.txt", "normal sibling")
        outside = self.root / "outside"
        self.write(outside / "sentinel.txt", "outside unchanged")
        self.redirect(base / name / "redirect", outside)
        expected = "ambiguous directory entry" if ";" in name else "redirect"
        self.assert_rejected_without_mutation(expected)
        self.assertEqual((outside / "sentinel.txt").read_text(), "outside unchanged")

    def test_source_bracket_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("[literal]")

    def test_destination_bracket_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("[literal]", destination=True)

    def test_source_semicolon_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("a;part")

    def test_destination_semicolon_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("a;part", destination=True)

    def test_source_trailing_semicolon_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("trailing;")

    def test_destination_trailing_semicolon_directory_cannot_hide_redirect(self):
        self.assert_named_redirect_rejected("trailing;", destination=True)

    def test_source_existing_sibling_cannot_hide_semicolon_redirect(self):
        self.assert_named_redirect_rejected("sibling;")

    def test_destination_existing_sibling_cannot_hide_semicolon_redirect(self):
        self.assert_named_redirect_rejected("sibling;", destination=True)

    if os.name != "nt":
        # These filename characters are valid on POSIX only. Windows does not
        # discover these cases, rather than reporting unexecuted checks as passes.
        def test_posix_colon_in_source_and_output_paths_is_preserved(self):
            renamed = self.root / "source:literal"
            self.source.rename(renamed)
            self.source = renamed
            self.build = renamed / "build"
            self.test_output = self.build / "tests" / "Debug"
            self.app_output = self.build / "application:literal"
            result = self.invoke()
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((self.app_output / "assets" / "current.txt").read_text(), "assets")

        def test_posix_glob_characters_are_literal_directory_names(self):
            for name in ("star*literal", "question?literal"):
                self.write(self.source / "assets" / name / "content.txt", name)
            result = self.invoke()
            self.assertEqual(result.returncode, 0, result.stderr)
            for name in ("star*literal", "question?literal"):
                self.assertEqual((self.test_output / "assets" / name / "content.txt").read_text(), name)

        def test_posix_source_star_directory_cannot_hide_redirect(self):
            self.assert_named_redirect_rejected("star*literal")

        def test_posix_destination_star_directory_cannot_hide_redirect(self):
            self.assert_named_redirect_rejected("star*literal", destination=True)

        def test_posix_source_question_directory_cannot_hide_redirect(self):
            self.assert_named_redirect_rejected("question?literal")

        def test_posix_destination_question_directory_cannot_hide_redirect(self):
            self.assert_named_redirect_rejected("question?literal", destination=True)


if __name__ == "__main__":
    unittest.main()
