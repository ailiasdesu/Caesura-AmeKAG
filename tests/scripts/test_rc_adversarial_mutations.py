#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_rc_adversarial_mutations.py — Adversarial Mutation Stress Test Suite for Release Candidate Gate

Author: Challenger 1 (Empirical Challenger Agent)
Purpose: Stress-test scripts/verify_release_candidate.py and scripts/compare_platform_parity.py
         against extensive adversarial mutations across manifests, checksums, parity snapshots,
         data leaks, and markdown reports. Asserts that EVERY mutation is caught and exits with code 1.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
VERIFIER_SCRIPT = REPO_ROOT / "scripts" / "verify_release_candidate.py"
COMPARATOR_SCRIPT = REPO_ROOT / "scripts" / "compare_platform_parity.py"


class MutationTestResult:
    def __init__(self, test_id: str, category: str, description: str, expected_exit: int, actual_exit: int, matched_error: bool, stdout: str, stderr: str):
        self.test_id = test_id
        self.category = category
        self.description = description
        self.expected_exit = expected_exit
        self.actual_exit = actual_exit
        self.matched_error = matched_error
        self.stdout = stdout
        self.stderr = stderr
        self.passed = (expected_exit == actual_exit) and matched_error


class TestReleaseCandidateAdversarialMutations(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.results: List[MutationTestResult] = []

    def create_sandbox(self) -> Tuple[Path, Path, Path, Path]:
        """Creates an isolated sandbox containing copies of artifacts/release and docs."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_rc_mut_"))
        
        # Mirror artifacts/release
        rel_dir = REPO_ROOT / "artifacts" / "release"
        if not rel_dir.exists() or not (rel_dir / "manifest.json").exists():
            subprocess.run([sys.executable, str(VERIFIER_SCRIPT), "--generate-bundle"], cwd=str(REPO_ROOT), check=True)
        sandbox_release = temp_dir / "artifacts" / "release"
        shutil.copytree(rel_dir, sandbox_release)
        
        # Mirror docs/status
        sandbox_docs = temp_dir / "docs" / "status"
        sandbox_docs.mkdir(parents=True, exist_ok=True)
        shutil.copy2(REPO_ROOT / "docs" / "status" / "release-candidate-report.md", sandbox_docs / "release-candidate-report.md")
        shutil.copy2(REPO_ROOT / "docs" / "status" / "platform-matrix.yaml", sandbox_docs / "platform-matrix.yaml")
        if (REPO_ROOT / "docs" / "status" / "platform-status.md").exists():
            shutil.copy2(REPO_ROOT / "docs" / "status" / "platform-status.md", sandbox_docs / "platform-status.md")

        # Mirror docs/platform
        sandbox_plat_docs = temp_dir / "docs" / "platform"
        sandbox_plat_docs.mkdir(parents=True, exist_ok=True)
        if (REPO_ROOT / "docs" / "platform" / "android-latest-head-validation.md").exists():
            shutil.copy2(REPO_ROOT / "docs" / "platform" / "android-latest-head-validation.md", sandbox_plat_docs / "android-latest-head-validation.md")
        if (REPO_ROOT / "docs" / "platform" / "ios-device-validation.md").exists():
            shutil.copy2(REPO_ROOT / "docs" / "platform" / "ios-device-validation.md", sandbox_plat_docs / "ios-device-validation.md")

        checksums_file = sandbox_release / "checksums" / "sha256sums.txt"
        report_file = sandbox_docs / "release-candidate-report.md"

        return temp_dir, sandbox_release, checksums_file, report_file

    def run_verifier(self, sandbox_release: Path, checksums_file: Path, report_file: Path, repo_root_override: Optional[Path] = None) -> Tuple[int, str, str]:
        """Runs verify_release_candidate.py against sandbox paths."""
        cmd = [
            sys.executable,
            str(VERIFIER_SCRIPT),
            "--artifacts-dir", str(sandbox_release),
            "--checksums-file", str(checksums_file),
            "--report-file", str(report_file),
        ]
        cwd = str(repo_root_override or REPO_ROOT)
        res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
        return res.returncode, res.stdout, res.stderr

    def run_comparator(self, parity_dir: Path, summary_file: Optional[Path] = None) -> Tuple[int, str, str]:
        """Runs compare_platform_parity.py against specified parity directory."""
        cmd = [
            sys.executable,
            str(COMPARATOR_SCRIPT),
            "--dir", str(parity_dir),
        ]
        if summary_file:
            cmd.extend(["--summary", str(summary_file)])
        res = subprocess.run(cmd, cwd=str(REPO_ROOT), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
        return res.returncode, res.stdout, res.stderr

    def record_and_assert(self, test_id: str, category: str, description: str, ret: int, out: str, err: str, expected_ret: int, expected_needle: str):
        matched = (expected_needle.lower() in out.lower() or expected_needle.lower() in err.lower())
        res = MutationTestResult(test_id, category, description, expected_ret, ret, matched, out, err)
        self.__class__.results.append(res)
        self.assertEqual(ret, expected_ret, f"[{test_id}] Exit code mismatch: expected {expected_ret}, got {ret}\nOutput: {out}\nError: {err}")
        self.assertTrue(matched, f"[{test_id}] Expected error substring '{expected_needle}' not found in output!\nOutput: {out}\nError: {err}")

    # =========================================================================
    # 0. Baseline Test
    # =========================================================================
    def test_00_baseline_untampered(self):
        """Baseline untampered release bundle MUST pass with exit code 0."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("BASE-01", "Baseline", "Untampered release candidate evidence bundle", ret, out, err, 0, "RC-GO (Approved")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # 1. Manifest Tampering Mutations
    # =========================================================================
    def test_mut_man_01_decision_maybe(self):
        """Manifest decision tampered to 'RC-MAYBE'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["decision"] = "RC-MAYBE"
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-01", "Manifest", "Decision tampered to 'RC-MAYBE'", ret, out, err, 1, "Manifest decision is not 'RC-GO'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_02_decision_nogo(self):
        """Manifest decision tampered to 'RC-NO-GO'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["decision"] = "RC-NO-GO"
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-02", "Manifest", "Decision tampered to 'RC-NO-GO'", ret, out, err, 1, "Manifest decision is not 'RC-GO'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_03_version_mismatch(self):
        """Manifest version tampered to '0.9.0-rc.1'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["version"] = "0.9.0-rc.1"
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-03", "Manifest", "Version tampered to '0.9.0-rc.1'", ret, out, err, 1, "Manifest version mismatch")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_04_commit_mismatch(self):
        """Manifest commit tampered to non-matching SHA."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["commit"] = "0000000000000000000000000000000000000000"
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-04", "Manifest", "Commit SHA tampered to all zeros", ret, out, err, 1, "commit mismatch")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_05_cpp_doctests_regressed(self):
        """Manifest C++ doctests total_cases regressed to 1050 (< 1052)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["cpp_doctests"]["total_cases"] = 1050
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-05", "Manifest", "C++ doctests count regressed (< 1052)", ret, out, err, 1, "Manifest C++ baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_06_cpp_doctests_failed(self):
        """Manifest C++ doctests failed_cases set to 2 (> 0)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["cpp_doctests"]["failed_cases"] = 2
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-06", "Manifest", "C++ doctests failed_cases > 0", ret, out, err, 1, "Manifest C++ baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_07_lua_suites_regressed(self):
        """Manifest Lua test suites total_suites regressed to 150 (< 158)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["lua_test_suites"]["total_suites"] = 150
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-07", "Manifest", "Lua test suites total_suites regressed (< 158)", ret, out, err, 1, "Manifest Lua baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_08_lua_suites_failed(self):
        """Manifest Lua test suites failed_suites set to 1 (> 0)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["lua_test_suites"]["failed_suites"] = 1
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-08", "Manifest", "Lua test suites failed_suites > 0", ret, out, err, 1, "Manifest Lua baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_09_coupling_violations(self):
        """Manifest module coupling violations set to 2 (> 0)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["module_coupling"]["violations"] = 2
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-09", "Manifest", "Module coupling violations > 0", ret, out, err, 1, "Manifest coupling baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_10_android_checks_regressed(self):
        """Manifest Android regression checks_passed regressed to 80 (< 88)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["baseline_test_suites"]["android_regression"]["checks_passed"] = 80
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-10", "Manifest", "Android regression checks regressed (< 88)", ret, out, err, 1, "Manifest Android regression baseline invalid")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_11_active_blocker_present(self):
        """Manifest active_blockers count set to 1 (> 0)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["release_blockers_review"]["active_blockers"] = 1
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-11", "Manifest", "Active blockers count > 0", ret, out, err, 1, "Release blockers not fully cleared")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_12_missing_required_blocker(self):
        """Manifest checklist missing 'crash_free' blocker."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            del data["release_blockers_review"]["checklist"]["crash_free"]
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-12", "Manifest", "Missing required blocker 'crash_free'", ret, out, err, 1, "Missing required blocker review item in manifest: crash_free")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_13_blocker_not_cleared(self):
        """Manifest blocker 'save_corruption_free' set to 'BLOCKED'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            data = json.loads(man_path.read_text(encoding="utf-8"))
            data["release_blockers_review"]["checklist"]["save_corruption_free"]["status"] = "BLOCKED"
            man_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-13", "Manifest", "Blocker status not CLEARED ('BLOCKED')", ret, out, err, 1, "Blocker item 'save_corruption_free' not CLEARED")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_14_corrupted_manifest_json(self):
        """Manifest file is invalid/corrupted JSON."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            man_path.write_text("{ this is corrupted json", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-14", "Manifest", "Corrupted manifest JSON syntax", ret, out, err, 1, "Failed to parse manifest.json")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_man_15_missing_manifest(self):
        """Manifest file is deleted / missing."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            man_path = s_rel / "manifest.json"
            man_path.unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-MAN-15", "Manifest", "Missing manifest.json file completely", ret, out, err, 1, "Missing release manifest")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # 2. Checksum & Cryptographic Integrity Mutations
    # =========================================================================
    def test_mut_chk_01_tampered_manifest_sha(self):
        """SHA-256 hash for manifest.json altered in sha256sums.txt."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            lines = s_chk.read_text(encoding="utf-8").splitlines()
            new_lines = []
            for line in lines:
                if "manifest.json" in line:
                    new_lines.append("ffff" + line[4:])
                else:
                    new_lines.append(line)
            s_chk.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-01", "Checksums", "Altered manifest.json SHA-256 in sha256sums.txt", ret, out, err, 1, "SHA-256 mismatch for artifacts/release/manifest.json")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_02_tampered_report_sha(self):
        """SHA-256 hash for cpp_test_report.json altered in sha256sums.txt."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            lines = s_chk.read_text(encoding="utf-8").splitlines()
            new_lines = []
            for line in lines:
                if "cpp_test_report.json" in line:
                    new_lines.append("0000" + line[4:])
                else:
                    new_lines.append(line)
            s_chk.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-02", "Checksums", "Altered cpp_test_report.json SHA-256 in sha256sums.txt", ret, out, err, 1, "SHA-256 mismatch for artifacts/release/reports/cpp_test_report.json")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_03_tampered_parity_sha(self):
        """SHA-256 hash for parity/windows.json altered in sha256sums.txt."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            lines = s_chk.read_text(encoding="utf-8").splitlines()
            new_lines = []
            for line in lines:
                if "parity/windows.json" in line:
                    new_lines.append("aaaa" + line[4:])
                else:
                    new_lines.append(line)
            s_chk.write_text("\n".join(new_lines) + "\n", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-03", "Checksums", "Altered parity/windows.json SHA-256 in sha256sums.txt", ret, out, err, 1, "SHA-256 mismatch for artifacts/release/parity/windows.json")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_04_missing_referenced_file(self):
        """sha256sums.txt references a non-existent file."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            content = s_chk.read_text(encoding="utf-8")
            content += "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  artifacts/release/non_existent_file.json\n"
            s_chk.write_text(content, encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-04", "Checksums", "Referenced non-existent file in sha256sums.txt", ret, out, err, 1, "Checksum referenced file not found")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_05_empty_sha256sums(self):
        """sha256sums.txt is empty (0 bytes)."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            s_chk.write_text("", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-05", "Checksums", "Empty sha256sums.txt file", ret, out, err, 1, "Checksums file is empty")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_06_missing_sha256sums(self):
        """sha256sums.txt is deleted / missing."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            s_chk.unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-06", "Checksums", "Missing sha256sums.txt file completely", ret, out, err, 1, "Missing checksums file")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_chk_07_malformed_checksum_line(self):
        """sha256sums.txt has a malformed single-token line."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            s_chk.write_text("just_a_single_invalid_hash_string_without_path\n", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-CHK-07", "Checksums", "Malformed checksum line without path", ret, out, err, 1, "Malformed checksum line")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # 3. Cross-Platform Parity Snapshot Mutations
    # =========================================================================
    def test_mut_par_01_windows_status_not_verified(self):
        """Windows parity snapshot status changed to 'probe'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            win_p = s_rel / "parity" / "windows.json"
            data = json.loads(win_p.read_text(encoding="utf-8"))
            data["status"] = "probe"
            win_p.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-01", "Parity", "Windows parity status set to 'probe'", ret, out, err, 1, "Platform windows parity status not 'verified'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_02_windows_route_a_ending_divergence(self):
        """Windows route_a ending changed from 'sunset' to 'midnight'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            win_p = s_rel / "parity" / "windows.json"
            data = json.loads(win_p.read_text(encoding="utf-8"))
            data["route_a"]["ending"] = "midnight"
            win_p.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-02", "Parity", "Windows route_a ending divergence ('midnight')", ret, out, err, 1, "Platform windows route_a ending not 'sunset'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_03_linux_route_b_ending_divergence(self):
        """Linux route_b ending changed from 'rain_shelter' to 'soaked'."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            lin_p = s_rel / "parity" / "linux.json"
            data = json.loads(lin_p.read_text(encoding="utf-8"))
            data["route_b"]["ending"] = "soaked"
            lin_p.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-03", "Parity", "Linux route_b ending divergence ('soaked')", ret, out, err, 1, "Platform linux route_b ending not 'rain_shelter'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_04_ios_status_not_gated(self):
        """iOS parity snapshot status changed to 'verified' without hardware proof."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            ios_p = s_rel / "parity" / "ios.json"
            data = json.loads(ios_p.read_text(encoding="utf-8"))
            data["status"] = "verified"
            ios_p.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-04", "Parity", "iOS parity status illegally set to 'verified'", ret, out, err, 1, "iOS parity status must be 'hardware-gated'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_05_missing_web_snapshot(self):
        """Missing web.json in parity directory."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            (s_rel / "parity" / "web.json").unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-05", "Parity", "Missing required web.json parity snapshot", ret, out, err, 1, "Missing parity snapshot for web")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_06_missing_ios_snapshot(self):
        """Missing ios.json in parity directory."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            (s_rel / "parity" / "ios.json").unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-06", "Parity", "Missing ios.json parity snapshot", ret, out, err, 1, "Missing iOS parity snapshot")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_par_07_corrupted_snapshot_json(self):
        """Corrupted JSON in android.json parity snapshot."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            (s_rel / "parity" / "android.json").write_text("{ not json", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-PAR-07", "Parity", "Corrupted android.json snapshot syntax", ret, out, err, 1, "Failed parsing parity snapshot")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # 4. Out-of-Sync Documentation & Report Bundle Mutations
    # =========================================================================
    def test_mut_doc_01_missing_rc_go_declaration(self):
        """Authoritative report missing 'RC-GO' declaration."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            doc_text = s_rep.read_text(encoding="utf-8")
            doc_text = doc_text.replace("RC-GO", "RC-PENDING")
            s_rep.write_text(doc_text, encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-01", "Documentation", "Authoritative report missing 'RC-GO' declaration", ret, out, err, 1, "Authoritative report does not contain definitive 'RC-GO' declaration")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_02_conflicting_rc_no_go_declaration(self):
        """Authoritative report contains conflicting 'RC-NO-GO' declaration."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            doc_text = s_rep.read_text(encoding="utf-8")
            doc_text += "\n\nWarning: We also declare RC-NO-GO due to pending items.\n"
            s_rep.write_text(doc_text, encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-02", "Documentation", "Authoritative report contains conflicting 'RC-NO-GO'", ret, out, err, 1, "Authoritative report contains conflicting 'RC-NO-GO' declaration")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_03_missing_commit_sha_in_report(self):
        """Authoritative report missing target commit SHA citation."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            doc_text = s_rep.read_text(encoding="utf-8")
            doc_text = re.sub(r"[0-9a-fA-F]{7,40}", "unknown_hash", doc_text)
            s_rep.write_text(doc_text, encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-03", "Documentation", "Authoritative report missing target commit SHA", ret, out, err, 1, "Authoritative report does not cite target commit SHA")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_04_missing_authoritative_report_file(self):
        """Authoritative report file deleted / missing."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            s_rep.unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-04", "Documentation", "Missing authoritative release candidate report file", ret, out, err, 1, "Missing authoritative release candidate report")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_05_missing_bundle_report_file(self):
        """Missing structured report file in artifacts/release/reports/."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            (s_rel / "reports" / "android_regression_report.md").unlink()

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-05", "Reports", "Missing android_regression_report.md in bundle", ret, out, err, 1, "Missing or empty release report")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_06_empty_bundle_report_file(self):
        """Empty (0 bytes) report file in artifacts/release/reports/."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            (s_rel / "reports" / "coupling_report.json").write_text("", encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-06", "Reports", "Empty 0-byte coupling_report.json in bundle", ret, out, err, 1, "Missing or empty release report")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_doc_07_platform_status_truncated(self):
        """platform-status.json has fewer than 6 platforms."""
        temp_dir, s_rel, s_chk, s_rep = self.create_sandbox()
        try:
            st_path = s_rel / "platform-status.json"
            data = json.loads(st_path.read_text(encoding="utf-8"))
            data["platforms"] = {"windows": data["platforms"]["windows"]}  # only 1 platform
            st_path.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_verifier(s_rel, s_chk, s_rep)
            self.record_and_assert("MUT-DOC-07", "Platform Status", "Truncated platform-status.json (< 6 platforms)", ret, out, err, 1, "platform-status.json has only 1 platforms, expected 6")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    # =========================================================================
    # 5. Direct Parity Comparator Adversarial Stress Tests
    # =========================================================================
    def test_mut_cmp_01_forbidden_gpu_key(self):
        """Parity snapshot contains leaked GPU key ('gpu_vendor')."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_cmp_mut_"))
        try:
            for f in (REPO_ROOT / "artifacts" / "release" / "parity").glob("*.json"):
                shutil.copy2(f, temp_dir / f.name)

            and_file = temp_dir / "android.json"
            data = json.loads(and_file.read_text(encoding="utf-8"))
            data["route_a"]["gpu_vendor"] = "Qualcomm Adreno 660"
            and_file.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_comparator(temp_dir)
            self.record_and_assert("MUT-CMP-01", "Comparator", "Data leak: Forbidden GPU key 'gpu_vendor'", ret, out, err, 1, "Forbidden key pattern 'gpu'")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_cmp_02_forbidden_pointer_address(self):
        """Parity snapshot contains leaked memory pointer value ('0x7ffe00112233')."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_cmp_mut_"))
        try:
            for f in (REPO_ROOT / "artifacts" / "release" / "parity").glob("*.json"):
                shutil.copy2(f, temp_dir / f.name)

            win_file = temp_dir / "windows.json"
            data = json.loads(win_file.read_text(encoding="utf-8"))
            data["route_a"]["native_handle"] = "0x7ffe00112233"
            win_file.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_comparator(temp_dir)
            self.record_and_assert("MUT-CMP-02", "Comparator", "Data leak: Forbidden native pointer address '0x7ffe00112233'", ret, out, err, 1, "Forbidden value pattern")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_cmp_03_forbidden_path_pattern(self):
        """Parity snapshot contains leaked OS absolute path ('/home/ubuntu/caesura/save.json')."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_cmp_mut_"))
        try:
            for f in (REPO_ROOT / "artifacts" / "release" / "parity").glob("*.json"):
                shutil.copy2(f, temp_dir / f.name)

            lin_file = temp_dir / "linux.json"
            data = json.loads(lin_file.read_text(encoding="utf-8"))
            data["route_a"]["save_path"] = "/home/ubuntu/caesura/save.json"
            lin_file.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_comparator(temp_dir)
            self.record_and_assert("MUT-CMP-03", "Comparator", "Data leak: Forbidden Linux absolute path '/home/ubuntu/...'", ret, out, err, 1, "Forbidden value pattern")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_cmp_04_missing_language_locale(self):
        """Web snapshot missing required Japanese ('ja') locale in route_a."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_cmp_mut_"))
        try:
            for f in (REPO_ROOT / "artifacts" / "release" / "parity").glob("*.json"):
                shutil.copy2(f, temp_dir / f.name)

            web_file = temp_dir / "web.json"
            data = json.loads(web_file.read_text(encoding="utf-8"))
            data["route_a"]["languages"] = ["zh", "en"]  # Missing 'ja'
            web_file.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_comparator(temp_dir)
            self.record_and_assert("MUT-CMP-04", "Comparator", "Missing language locale 'ja' in route_a", ret, out, err, 1, "route_a.languages")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)

    def test_mut_cmp_05_cross_platform_divergence(self):
        """Android route_b ending diverges to 'divergent_rain_ending'."""
        temp_dir = Path(tempfile.mkdtemp(prefix="caesura_cmp_mut_"))
        try:
            for f in (REPO_ROOT / "artifacts" / "release" / "parity").glob("*.json"):
                shutil.copy2(f, temp_dir / f.name)

            and_file = temp_dir / "android.json"
            data = json.loads(and_file.read_text(encoding="utf-8"))
            data["route_b"]["ending"] = "divergent_rain_ending"
            and_file.write_text(json.dumps(data, indent=2), encoding="utf-8")

            ret, out, err = self.run_comparator(temp_dir)
            self.record_and_assert("MUT-CMP-05", "Comparator", "Cross-platform divergence in route_b ending", ret, out, err, 1, "Cross-platform divergence between windows and android")
        finally:
            shutil.rmtree(temp_dir, ignore_errors=True)


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestReleaseCandidateAdversarialMutations)
    runner = unittest.TextTestRunner(verbosity=2)
    test_result = runner.run(suite)
    
    print("\n" + "=" * 100)
    print(f"{'ID':<12} | {'Category':<14} | {'Expected':<8} | {'Actual':<8} | {'Matched Error':<14} | {'Status':<6} | {'Description'}")
    print("-" * 100)
    for r in TestReleaseCandidateAdversarialMutations.results:
        st = "PASS" if r.passed else "FAIL"
        print(f"{r.test_id:<12} | {r.category:<14} | {r.expected_exit:<8} | {r.actual_exit:<8} | {str(r.matched_error):<14} | {st:<6} | {r.description}")
    print("=" * 100)
    print(f"Total Mutation Tests Run: {len(TestReleaseCandidateAdversarialMutations.results)}")
    all_passed = all(r.passed for r in TestReleaseCandidateAdversarialMutations.results)
    print(f"Overall Empirical Result: {'ALL MUTATIONS CAUGHT & REJECTED (100% PASS)' if all_passed else 'SOME MUTATIONS MISSED'}")
    sys.exit(0 if test_result.wasSuccessful() else 1)
