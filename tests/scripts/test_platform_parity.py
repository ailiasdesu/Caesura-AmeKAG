#!/usr/bin/env python3
"""
Unit and regression tests for scripts/compare_platform_parity.py.
Validates parity assertion logic, schema verification, anti-leakage checks,
hardware-gate handling, and cross-platform equivalence.
"""

import copy
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

# Add repo root to sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT))

import scripts.compare_platform_parity as comparator


class TestPlatformParityComparator(unittest.TestCase):
    def setUp(self):
        self.temp_dir = Path(tempfile.mkdtemp(prefix="caesura_parity_test_"))
        # Copy real artifacts to temp_dir for testing
        real_artifacts = REPO_ROOT / "artifacts" / "parity"
        for f in real_artifacts.glob("*.json"):
            if f.name != "parity_summary.json":
                shutil.copy2(f, self.temp_dir / f.name)

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_canonical_artifacts_pass(self):
        """Validates that real production parity artifacts in artifacts/parity pass cleanly."""
        summary_file = self.temp_dir / "summary.json"
        success = comparator.run_parity_comparison(
            parity_dir=self.temp_dir,
            summary_out=summary_file,
            strict=False,
        )
        self.assertTrue(success, "Production parity artifacts must pass comparison.")
        self.assertTrue(summary_file.exists(), "Summary JSON should be written.")
        
        with open(summary_file, "r", encoding="utf-8") as f:
            summary = json.load(f)
        
        self.assertEqual(summary["overall_status"], "PASS")
        self.assertEqual(summary["verified_count"], 4)
        self.assertEqual(summary["gated_count"], 1)
        self.assertEqual(summary["failed_count"], 0)
        self.assertEqual(summary["platforms"]["windows"]["result"], "PASS")
        self.assertEqual(summary["platforms"]["linux"]["result"], "PASS")
        self.assertEqual(summary["platforms"]["web"]["result"], "PASS")
        self.assertEqual(summary["platforms"]["android"]["result"], "PASS")
        self.assertEqual(summary["platforms"]["ios"]["result"], "GATED (Honest)")

    def test_missing_required_platform_fails(self):
        """Missing a tier-1 required platform must fail the suite."""
        linux_json = self.temp_dir / "linux.json"
        if linux_json.exists():
            linux_json.unlink()

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertFalse(success, "Missing required platform must fail.")

    def test_route_choice_mismatch_fails(self):
        """Tampering with route choice or ending must be detected as mismatch."""
        win_file = self.temp_dir / "windows.json"
        with open(win_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        data["route_a"]["flag_is_sun"] = 0  # Divergence
        with open(win_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertFalse(success, "Mismatched flag_is_sun must cause failure.")

    def test_missing_language_fails(self):
        """Missing a required localization language must cause failure."""
        web_file = self.temp_dir / "web.json"
        with open(web_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        data["route_a"]["languages"] = ["zh", "en"]  # Missing 'ja'
        with open(web_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertFalse(success, "Missing language locale must cause failure.")

    def test_data_leakage_forbidden_keys(self):
        """Forbidden GPU / hardware / pointer keys must be flagged as sanitization leaks."""
        android_file = self.temp_dir / "android.json"
        with open(android_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        data["route_a"]["gpu_renderer"] = "Adreno 660"  # Leak
        with open(android_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

        leaks = comparator.sanitize_check_obj(data)
        self.assertTrue(any("gpu" in leak for leak in leaks), f"Expected GPU leak detected, got: {leaks}")

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertFalse(success, "Parity comparison must fail when data leaks are present.")

    def test_data_leakage_forbidden_values(self):
        """Forbidden OS absolute paths or memory pointers must be detected."""
        test_obj = {
            "platform": "windows",
            "save_path": "C:\\Users\\Admin\\AppData\\Local\\save.json",
        }
        leaks = comparator.sanitize_check_obj(test_obj)
        self.assertTrue(len(leaks) > 0, "Absolute path leak must be detected.")

        pointer_obj = {
            "window_handle": "0x7ffee4a10020",
        }
        leaks2 = comparator.sanitize_check_obj(pointer_obj)
        self.assertTrue(len(leaks2) > 0, "Native pointer address leak must be detected.")

    def test_hardware_gated_honest_reporting(self):
        """Hardware-gated targets (e.g. iOS) with valid gate documentation do not fail CI."""
        ios_file = self.temp_dir / "ios.json"
        with open(ios_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        self.assertEqual(data["status"], "hardware-gated")
        self.assertIn("gate_reason", data["evidence"])

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertTrue(success, "Honest hardware gate should not fail overall CI pass.")

    def test_invalid_schema_structure(self):
        """Invalid schema structure (e.g. missing evidence, missing platform) fails."""
        data_invalid = {
            "story": "first_vn",
            "status": "verified",
            # missing platform and evidence
        }
        errs = comparator.validate_snapshot_structure(data_invalid, Path("invalid.json"))
        self.assertTrue(len(errs) >= 2, f"Schema validation should catch missing fields: {errs}")

    def test_cross_platform_divergence_check(self):
        """Two verified platforms having different route states must trigger cross-platform divergence failure."""
        and_file = self.temp_dir / "android.json"
        with open(and_file, "r", encoding="utf-8") as f:
            data = json.load(f)

        data["route_b"]["ending"] = "alternate_rain_ending"
        with open(and_file, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)

        success = comparator.run_parity_comparison(parity_dir=self.temp_dir)
        self.assertFalse(success, "Cross-platform divergence must fail.")

    def test_cli_execution(self):
        """Tests that compare_platform_parity.py runs cleanly via subprocess CLI."""
        summary_path = self.temp_dir / "cli_summary.json"
        cmd = [
            sys.executable,
            str(REPO_ROOT / "scripts" / "compare_platform_parity.py"),
            "--dir", str(self.temp_dir),
            "--summary", str(summary_path),
        ]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
        self.assertEqual(res.returncode, 0, f"CLI exited non-zero: {res.stderr}\n{res.stdout}")
        self.assertIsNotNone(res.stdout)
        self.assertIn("RESULT: PASS", res.stdout)
        self.assertTrue(summary_path.exists())


if __name__ == "__main__":
    unittest.main()
