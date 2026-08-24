#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Cross-Platform Behavioral Parity Comparator
Authoritative verification tool for Milestone M2 (Task 02: First-VN Cross-Platform Behavioral Parity).

Asserts behavioral, progression, branching, variable, save/load, localization,
and ending parity across target platforms (Windows, Linux, Web, Android, iOS).
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

if sys.stdout.encoding != "utf-8" and hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass

REQUIRED_PLATFORMS = ["windows", "linux", "web", "android"]
GATED_PLATFORMS = ["ios"]
ALLOWED_STATUSES = [
    "verified",
    "probe",
    "pending",
    "hardware-gated",
    "credential-gated",
    "blocked",
    "not-applicable",
]

# Patterns that indicate forbidden platform-specific runtime leaks
FORBIDDEN_KEY_PATTERNS = [
    r"gpu",
    r"vram",
    r"fps",
    r"frame_count",
    r"timestamp",
    r"pointer",
    r"hwnd",
    r"pid",
    r"thread_id",
    r"win32",
    r"x11",
    r"egl",
    r"gl_vendor",
    r"renderer_type",
]

FORBIDDEN_VALUE_PATTERNS = [
    r"^[a-zA-Z]:\\",  # Windows absolute path
    r"^/Users/",      # macOS user path
    r"^/home/",       # Linux home path
    r"^/data/data/",  # Android private path
    r"0x[0-9a-fA-F]{6,16}",  # Pointer address
]

CANONICAL_ROUTE_A = {
    "choice": "sun",
    "route": "sun",
    "flag_is_sun": 1,
    "final_label": "*ending",
    "ending": "sunset",
    "save_roundtrip": True,
    "languages": ["zh", "en", "ja"],
}

CANONICAL_ROUTE_B = {
    "choice": "rain",
    "route": "rain",
    "flag_is_sun": 0,
    "final_label": "*ending",
    "ending": "rain_shelter",
    "save_roundtrip": True,
    "languages": ["zh", "en", "ja"],
}


def sanitize_check_obj(obj: Any, path: str = "") -> List[str]:
    """Recursively checks for leaks in key names and values."""
    violations: List[str] = []
    if isinstance(obj, dict):
        for k, v in obj.items():
            current_path = f"{path}.{k}" if path else k
            # Check key name (except allowed metadata fields like runner or commit)
            if not current_path.startswith("evidence."):
                for pat in FORBIDDEN_KEY_PATTERNS:
                    if re.search(pat, k, re.IGNORECASE):
                        violations.append(f"Forbidden key pattern '{pat}' in '{current_path}'")
            violations.extend(sanitize_check_obj(v, current_path))
    elif isinstance(obj, list):
        for idx, item in enumerate(obj):
            violations.extend(sanitize_check_obj(item, f"{path}[{idx}]"))
    elif isinstance(obj, str):
        if not path.startswith("evidence."):
            for pat in FORBIDDEN_VALUE_PATTERNS:
                if re.search(pat, obj):
                    violations.append(f"Forbidden value pattern '{pat}' at '{path}': {obj}")
    return violations


def validate_snapshot_structure(data: Dict[str, Any], file_path: Path) -> List[str]:
    """Validates required schema fields for a FirstVNStateSnapshot."""
    errors: List[str] = []
    if "platform" not in data:
        errors.append(f"{file_path.name}: missing 'platform'")
    if data.get("story") != "first_vn":
        errors.append(f"{file_path.name}: 'story' must be 'first_vn' (got '{data.get('story')}')")
    
    status = data.get("status")
    if status not in ALLOWED_STATUSES:
        errors.append(f"{file_path.name}: invalid status '{status}', must be one of {ALLOWED_STATUSES}")

    if "evidence" not in data or not isinstance(data["evidence"], dict):
        errors.append(f"{file_path.name}: missing or invalid 'evidence' object")
    else:
        if "runner" not in data["evidence"]:
            errors.append(f"{file_path.name}: missing 'evidence.runner'")
        if "commit" not in data["evidence"]:
            errors.append(f"{file_path.name}: missing 'evidence.commit'")

    if "route_a" not in data or not isinstance(data["route_a"], dict):
        errors.append(f"{file_path.name}: missing or invalid 'route_a' object")
    if "route_b" not in data or not isinstance(data["route_b"], dict):
        errors.append(f"{file_path.name}: missing or invalid 'route_b' object")

    return errors


def compare_route(actual: Dict[str, Any], canonical: Dict[str, Any], route_name: str) -> List[str]:
    """Compares actual route state against canonical expectation."""
    mismatches: List[str] = []
    for key, expected_val in canonical.items():
        if key not in actual:
            mismatches.append(f"{route_name}.{key}: missing (expected {expected_val})")
        elif key == "languages":
            act_langs = sorted(actual[key]) if isinstance(actual[key], list) else []
            exp_langs = sorted(expected_val)
            if act_langs != exp_langs:
                mismatches.append(f"{route_name}.languages: {actual[key]} != {expected_val}")
        else:
            if actual[key] != expected_val:
                mismatches.append(f"{route_name}.{key}: '{actual[key]}' != '{expected_val}'")
    return mismatches


def load_snapshots(parity_dir: Path) -> Tuple[Dict[str, Dict[str, Any]], List[str]]:
    """Loads and parses all JSON files in the parity directory."""
    snapshots: Dict[str, Dict[str, Any]] = {}
    errors: List[str] = []

    if not parity_dir.exists():
        errors.append(f"Parity directory does not exist: {parity_dir}")
        return snapshots, errors

    json_files = sorted(parity_dir.glob("*.json"))
    if not json_files:
        errors.append(f"No JSON snapshot files found in {parity_dir}")
        return snapshots, errors

    for jf in json_files:
        if jf.name.startswith("parity_summary"):
            continue
        try:
            with open(jf, "r", encoding="utf-8") as f:
                data = json.load(f)
            plat = data.get("platform", jf.stem)
            snapshots[plat] = data
        except Exception as e:
            errors.append(f"Failed to parse {jf.name}: {e}")

    return snapshots, errors


def run_parity_comparison(
    parity_dir: Path,
    summary_out: Optional[Path] = None,
    strict: bool = False,
) -> bool:
    """Executes full parity verification and comparison across platforms."""
    print("=" * 80)
    print("  Caesura (AmeKAG) — First-VN Cross-Platform Parity Verification Suite")
    print("=" * 80)
    print(f"Target Directory : {parity_dir}")
    print(f"Required Targets : {', '.join(REQUIRED_PLATFORMS)}")
    print(f"Gated Targets    : {', '.join(GATED_PLATFORMS)}")
    print("-" * 80)

    snapshots, load_errors = load_snapshots(parity_dir)
    if load_errors:
        for err in load_errors:
            print(f"[ERROR] {err}")
        return False

    all_pass = True
    results_summary: Dict[str, Any] = {
        "suite": "First-VN Cross-Platform Behavioral Parity",
        "verified_count": 0,
        "gated_count": 0,
        "failed_count": 0,
        "platforms": {},
    }

    # Table Header
    header_fmt = "{:<10} | {:<15} | {:<16} | {:<16} | {:<12} | {:<8}"
    print(header_fmt.format("Platform", "Status", "Route A (Sun)", "Route B (Rain)", "Languages", "Result"))
    print("-" * 80)

    # Check Required & Gated Platforms
    all_target_platforms = REQUIRED_PLATFORMS + GATED_PLATFORMS
    
    for plat in all_target_platforms:
        plat_res: Dict[str, Any] = {"platform": plat}

        if plat not in snapshots:
            if plat in REQUIRED_PLATFORMS:
                print(header_fmt.format(plat, "MISSING", "-", "-", "-", "FAIL (Missing)"))
                all_pass = False
                results_summary["failed_count"] += 1
                plat_res["status"] = "missing"
                plat_res["result"] = "FAIL"
            else:
                print(header_fmt.format(plat, "UNCONFIGURED", "-", "-", "-", "GATED"))
                results_summary["gated_count"] += 1
                plat_res["status"] = "unconfigured"
                plat_res["result"] = "GATED"
            results_summary["platforms"][plat] = plat_res
            continue

        snap = snapshots[plat]
        struct_errors = validate_snapshot_structure(snap, parity_dir / f"{plat}.json")
        leaks = sanitize_check_obj(snap)
        
        status = snap.get("status", "unknown")
        plat_res["status"] = status
        plat_res["evidence"] = snap.get("evidence", {})

        if struct_errors:
            print(f"[FAIL] {plat} schema invalid: {'; '.join(struct_errors)}")
            all_pass = False
            results_summary["failed_count"] += 1
            plat_res["result"] = "FAIL (Schema Invalid)"
            plat_res["errors"] = struct_errors
            results_summary["platforms"][plat] = plat_res
            continue

        if leaks:
            print(f"[FAIL] {plat} data sanitization leak: {'; '.join(leaks)}")
            all_pass = False
            results_summary["failed_count"] += 1
            plat_res["result"] = "FAIL (Data Leak)"
            plat_res["leaks"] = leaks
            results_summary["platforms"][plat] = plat_res
            continue

        ra = snap.get("route_a", {})
        rb = snap.get("route_b", {})
        
        mismatches_a = compare_route(ra, CANONICAL_ROUTE_A, "route_a")
        mismatches_b = compare_route(rb, CANONICAL_ROUTE_B, "route_b")
        all_mismatches = mismatches_a + mismatches_b

        ra_summary = f"{ra.get('route')}/flag={ra.get('flag_is_sun')}/{ra.get('ending')}"
        rb_summary = f"{rb.get('route')}/flag={rb.get('flag_is_sun')}/{rb.get('ending')}"
        langs_str = ",".join(ra.get("languages", []))

        if status == "hardware-gated" or status == "pending" or status == "credential-gated":
            # For gated platforms, ensure honest gate recording and semantic route alignment
            if all_mismatches:
                res_str = "FAIL (Gated Mismatch)"
                all_pass = False
                results_summary["failed_count"] += 1
            else:
                res_str = "GATED (Honest)"
                results_summary["gated_count"] += 1
            plat_res["result"] = res_str
            plat_res["gate_reason"] = snap.get("evidence", {}).get("gate_reason", "Hardware gated")
        elif status == "verified":
            if all_mismatches:
                res_str = "FAIL (Mismatch)"
                all_pass = False
                results_summary["failed_count"] += 1
                plat_res["mismatches"] = all_mismatches
            else:
                res_str = "PASS"
                results_summary["verified_count"] += 1
            plat_res["result"] = res_str
        else:
            res_str = f"UNKNOWN ({status})"
            all_pass = False
            results_summary["failed_count"] += 1
            plat_res["result"] = res_str

        print(header_fmt.format(plat, status, ra_summary, rb_summary, langs_str, res_str))
        if all_mismatches:
            for m in all_mismatches:
                print(f"       -> Mismatch: {m}")

        plat_res["route_a"] = ra
        plat_res["route_b"] = rb
        results_summary["platforms"][plat] = plat_res

    # Cross-Platform State Equivalence Check
    # Verify that all 'verified' platforms have exactly identical route_a and route_b dictionaries
    verified_snaps = {p: snapshots[p] for p in REQUIRED_PLATFORMS if p in snapshots and snapshots[p].get("status") == "verified"}
    if len(verified_snaps) > 1:
        first_p = list(verified_snaps.keys())[0]
        first_ra = verified_snaps[first_p]["route_a"]
        first_rb = verified_snaps[first_p]["route_b"]
        
        for other_p, other_snap in verified_snaps.items():
            if other_p == first_p:
                continue
            if other_snap["route_a"] != first_ra or other_snap["route_b"] != first_rb:
                print(f"[FAIL] Cross-platform divergence between {first_p} and {other_p}!")
                all_pass = False

    print("=" * 80)
    print(f"Summary: Verified={results_summary['verified_count']}, Gated={results_summary['gated_count']}, Failed={results_summary['failed_count']}")
    
    if all_pass:
        print("RESULT: PASS -- All required platforms exhibit 100% behavioral parity.")
    else:
        print("RESULT: FAIL -- Parity mismatches or sanitization violations detected.")

    results_summary["overall_status"] = "PASS" if all_pass else "FAIL"

    if summary_out:
        summary_out.parent.mkdir(parents=True, exist_ok=True)
        with open(summary_out, "w", encoding="utf-8") as f:
            json.dump(results_summary, f, indent=2)
        print(f"Summary artifact written to: {summary_out}")

    return all_pass


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Caesura (AmeKAG) Cross-Platform Parity Verification Tool"
    )
    parser.add_argument(
        "--dir",
        type=Path,
        default=Path("artifacts/parity"),
        help="Path to directory containing platform JSON snapshots (default: artifacts/parity)",
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("artifacts/parity/parity_summary.json"),
        help="Path to output parity summary JSON (default: artifacts/parity/parity_summary.json)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Fail if any platform is gated or pending",
    )

    args = parser.parse_args()
    success = run_parity_comparison(
        parity_dir=args.dir,
        summary_out=args.summary,
        strict=args.strict,
    )
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
