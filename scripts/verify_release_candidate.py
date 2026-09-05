#!/usr/bin/env python3
"""Read-only validation of real execution evidence against a trusted receipt.

Legacy RC bundles contain synthetic PASS values and are not execution evidence.
They remain untouched. Use run_validation.py, then collect_validation_evidence.py
or --generate-bundle --run RECEIPT to assemble actual reports. This tool verifies
the selected profile; it neither grants publication approval nor emits RC-GO.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

from collect_validation_evidence import (
    EvidenceError, SOURCE_RE, build_manifest, collect_evidence, contained_path, digest, read_json, require,
)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RELEASE_DIR = ROOT / "artifacts" / "release"


def get_target_commit(repo_root: Path, override: str | None = None) -> tuple[str, str]:
    """Resolve source identity; never fall back to a historic hard-coded commit."""
    if override is not None:
        require(SOURCE_RE.fullmatch(override) is not None, "--commit must be a full lowercase commit SHA")
        return override, override[:8]
    require(any((parent / ".git").exists() for parent in (repo_root, *repo_root.parents)), "No Git repository; provide --commit")
    result = subprocess.run(["git", "rev-parse", "HEAD"], cwd=repo_root,
                            capture_output=True, text=True, encoding="utf-8", check=False)
    require(result.returncode == 0 and SOURCE_RE.fullmatch(result.stdout.strip()) is not None,
            "Cannot resolve source commit; provide --commit")
    full = result.stdout.strip()
    return full, full[:8]


def verify_evidence(output: Path, profile_path: Path, profile_name: str,
                    expected_run: Path, *, source_sha: str, release: bool = True) -> list[str]:
    """Reparse original reports, bound to external executor metadata and profile.

    `expected_run` must be selected by the caller from the controlled execution
    boundary, never taken from the evidence manifest being checked. For hosted
    releases the caller authenticates the Actions workflow/run/attempt/artifact.
    """
    try:
        require(SOURCE_RE.fullmatch(source_sha) is not None, "Invalid expected source SHA")
        manifest = read_json(contained_path(output / "manifest.json", output))
        require(manifest.get("kind") == "caesura-validation-evidence",
                "Legacy bundle is UNVERIFIED: missing execution provenance; preserve it and collect a real run")
        require(output.resolve() not in expected_run.resolve().parents,
                "Expected execution receipt must come from outside the evidence bundle")
        require(output.resolve() not in profile_path.resolve().parents,
                "Trusted profile must come from outside the evidence bundle")
        reconstructed, _ = build_manifest(profile_path, profile_name, expected_run, collected_root=output)
        run = reconstructed["context"]
        require(run.get("source_sha") == source_sha, "Wrong source SHA in expected execution receipt")
        require(digest(contained_path(output / "execution-receipt.json", output)) == reconstructed["receipt_sha256"],
                "Execution receipt mismatch (source/workflow/run/run_attempt or artifacts changed)")
        require(digest(contained_path(output / "profile.json", output)) == reconstructed["profile_sha256"], "Trusted profile snapshot mismatch")
        require(manifest == reconstructed, "Evidence manifest differs from actual reports and trusted execution receipt")
        errors = []
        if run.get("source_changed_during_run"):
            errors.append("Source changed during execution")
        if run.get("fixtures_changed_during_run"):
            errors.append("Fixture inputs changed during execution")
        if release and run.get("purpose") != "validation":
            errors.append("test-fixture evidence cannot be used for release verification")
        if release and run.get("dirty"):
            errors.append("dirty worktree evidence is diagnostic only, not clean source release evidence")
        for check in reconstructed["checks"]:
            if check["required"] and check["result"] != "PASS":
                errors.append(f"Required check {check['id']}: {check['result']}: " + "; ".join(check["reasons"]))
        return errors
    except (EvidenceError, OSError, TypeError, KeyError) as error:
        return [str(error)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts-dir", type=Path, default=DEFAULT_RELEASE_DIR)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--profile-name")
    parser.add_argument("--expected-run", type=Path, help="Separately trusted controlled-executor receipt")
    parser.add_argument("--run", type=Path, help="Raw receipt to collect with --generate-bundle")
    parser.add_argument("--generate-bundle", action="store_true", help="Assemble actual logs from --run into a NEW evidence directory")
    parser.add_argument("--commit")
    parser.add_argument("--repo-root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true", help="Compatibility flag; verification is always strict")
    parser.add_argument("--diagnostic", action="store_true", help="Permit dirty or test-fixture evidence; never grants release approval")
    parser.add_argument("--skip-if-missing", action="store_true", help="Absent optional evidence exits 77 (SKIP), never a successful gate")
    parser.add_argument("--checksums-file", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--report-file", type=Path, help=argparse.SUPPRESS)
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()
    try:
        if not args.generate_bundle and not (args.artifacts_dir / "manifest.json").exists() and args.skip_if_missing:
            print("EVIDENCE VERIFICATION SKIP: no bundle; run run_validation.py and collect its execution receipt")
            return 77
        require(not args.checksums_file and not args.report_file,
                "Legacy report/checksum flags cannot prove execution. Supply --profile, --profile-name and --expected-run")
        require(args.profile is not None and bool(args.profile_name) and args.expected_run is not None,
                "Required: --profile FILE --profile-name NAME --expected-run RAW/run.json")
        source, _ = get_target_commit(args.repo_root, args.commit)
        if args.generate_bundle:
            require(args.run is not None, "--generate-bundle requires --run from a real controlled execution")
            collect_evidence(args.profile, args.profile_name, args.run, args.artifacts_dir)
        errors = verify_evidence(args.artifacts_dir, args.profile, args.profile_name,
                                 args.expected_run, source_sha=source, release=not args.diagnostic)
        for error in errors:
            print(f"EVIDENCE VERIFICATION FAIL: {error}", file=sys.stderr)
        if errors:
            return 1
        print(f"EVIDENCE VERIFICATION PASS: profile={args.profile_name} source={source}")
        print("Scope: this execution profile only; publication approval and unexecuted platforms are not implied.")
        return 0
    except (EvidenceError, OSError) as error:
        print(f"EVIDENCE VERIFICATION FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
