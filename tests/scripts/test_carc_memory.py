"""Run real CARC allocation failures in Windows Job Object child processes."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


class CarcMemoryTests(unittest.TestCase):
    probe: Path
    artifact_dir: Path | None = None
    fixture_dir: Path
    temporary: tempfile.TemporaryDirectory[str]

    @classmethod
    def run_probe(cls, name: str, *arguments: str) -> subprocess.CompletedProcess[str]:
        started = time.time()
        result = subprocess.run(
            [str(cls.probe), *arguments],
            cwd=cls.fixture_dir,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
            creationflags=subprocess.CREATE_NO_WINDOW,
            check=False,
        )
        if cls.artifact_dir is not None:
            record = {
                "arguments": [str(cls.probe), *arguments],
                "started_unix": started,
                "elapsed_seconds": time.time() - started,
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr,
            }
            (cls.artifact_dir / f"{name}.json").write_text(
                json.dumps(record, indent=2), encoding="utf-8"
            )
        print(f"{name}:\n{result.stdout}", flush=True)
        return result

    @classmethod
    def setUpClass(cls) -> None:
        if os.name != "nt":
            raise RuntimeError("CARC memory probe must only be registered on Windows")
        if not cls.probe.is_file():
            raise FileNotFoundError(cls.probe)
        cls.temporary = tempfile.TemporaryDirectory(prefix="caesura_carc_memory_")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.fixture_dir = Path(cls.temporary.name)
        if cls.artifact_dir is not None:
            cls.artifact_dir.mkdir(parents=True, exist_ok=True)
        result = cls.run_probe("prepare", "prepare")
        if result.returncode:
            raise RuntimeError(f"Fixture creation failed: {result.stdout}\n{result.stderr}")
        if "\tsignature_valid" not in result.stdout or "\tcanonical_exact" not in result.stdout:
            raise AssertionError("Fixture validation markers missing")

    def assert_bounded_contract(self, phase: str, state: str) -> None:
        result = self.run_probe(phase, "launch", phase)
        output = result.stdout
        self.assertEqual(result.returncode, 0, output + result.stderr)
        self.assertNotIn("std::bad_alloc", output)
        self.assertIn(f"RESULT\t{phase}\treturned\t0\n", output)
        self.assertIn(f"STATE\t{state}\n", output)
        self.assertIn("RECOVERY\t1\t1\t1\t1\n", output)
        self.assertIn("CONTRACT\t1\n", output)
        self.assertIn("CHILD_EXIT\t0\t0\n", output)
        limit_rows = [line.split("\t") for line in output.splitlines() if line.startswith("LIMIT\t")]
        self.assertEqual(len(limit_rows), 1, output)
        baseline, limit = map(int, limit_rows[0][1:])
        self.assertGreater(baseline, 0)
        self.assertEqual(limit - baseline, 8 * 1024 * 1024)
        self.assertLess(limit, 128 * 1024 * 1024)  # below the outer child-process cap

    def test_read_allocation_failure_returns_empty_and_preserves_reader(self) -> None:
        self.assert_bounded_contract("read", "1\t2\t1\t1")

    def test_open_allocation_failure_returns_false_and_clears_reader(self) -> None:
        self.assert_bounded_contract("open", "0\t0\t0\t0")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--artifact-dir", type=Path)
    args = parser.parse_args()
    CarcMemoryTests.probe = args.probe.resolve()
    CarcMemoryTests.artifact_dir = args.artifact_dir.resolve() if args.artifact_dir else None
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(CarcMemoryTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
