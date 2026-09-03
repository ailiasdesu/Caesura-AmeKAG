#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
generate_plan_status.py — auto-generate the repo-fact section (t196)

Writes/refreshes the fact block of docs/plans/2026-09-03-032-advanced-stage-backlog.md:

    <!-- plan-status:generated -->
    ## 事实状态（自动生成 — 勿手改）
    ...facts...
    <!-- /plan-status:generated -->

The manual sections (directions / adjudication records) of the document are
NEVER touched. Facts are static, zero-network repository truths:

  a. capability-closure counts (parsed from the matrix stats line);
  b. Node migration status (package_game.mjs + PackagingService findNode /
     no-findGitBash + caesura_build find_node + package_distribution _find_node);
  c. vendored-Lua Windows UTF-8 widening (four sites: liolib/lua/lauxlib/loadlib).

The block embeds NO timestamp (generated_at omitted by design): same input
yields byte-identical output -- idempotent by construction. --check compares
the on-disk block against the freshly computed one and exits 1 on any drift
(a missing marker block also exits 1 with a fix hint), mirroring the
generate_platform_status.py --check style.
"""

import argparse
import difflib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLAN_PATH = ROOT / "docs" / "plans" / "2026-09-03-032-advanced-stage-backlog.md"
MATRIX_PATH = ROOT / "docs" / "design" / "capability-closure-matrix.md"
NODE_SOURCES = [
    ("scripts/package_game.mjs", "exists"),
    ("src/rpc/services/PackagingService.cpp", "findNode and no findGitBash"),
    ("scripts/caesura_build.py", "find_node"),
    ("scripts/package_distribution.py", "_find_node"),
]
UNICODE_SITES = [
    ("external/lua/liolib.c", "lua_fopen_utf8"),
    ("external/lua/lua.c", "wmain"),
    ("external/lua/lauxlib.c", "lauxlib_fopen_utf8"),
    ("external/lua/loadlib.c", "loadlib_fopen_utf8"),
]

OPEN_MARK = "<!-- plan-status:generated -->"
CLOSE_MARK = "<!-- /plan-status:generated -->"
SECTION_TITLE = "## 事实状态（自动生成 — 勿手改）"


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


def _exists(path: Path) -> bool:
    return path.exists()


def fact_closure(root: Path = ROOT, matrix_path: Path = None):
    """Return (status_line, evidence). Counts parsed from the matrix header."""
    mat = matrix_path or (root / "docs" / "design" / "capability-closure-matrix.md")
    text = _read(mat)
    if not text:
        return ("closure: matrix MISSING (generator pending)",
                "docs/design/capability-closure-matrix.md absent or empty")
    line = None
    for l in text.splitlines():
        if "UNWIRED：" in l and "PARTIAL：" in l:
            line = l
            break
    if line is None:
        return ("closure: matrix stats line MISSING",
                "no UNWIRED/..PARTIAL stats line in matrix header")

    def pick(key: str) -> str:
        # EXPERIMENTAL appears as "EXPERIMENTAL(人工)：4" in the stats line.
        m = re.search(re.escape(key) + r"[^：]*：\s*(\d+)", line)
        return m.group(1) if m else "?"

    status = ("closure: PARTIAL=" + pick("PARTIAL")
              + " / CLOSED=" + pick("CLOSED")
              + " / UNWIRED=" + pick("UNWIRED")
              + " / EXTRA=" + pick("EXTRA")
              + " / EXPERIMENTAL=" + pick("EXPERIMENTAL"))
    ln = text.splitlines().index(line) + 1
    return status, "docs/design/capability-closure-matrix.md:%d (stats line)" % ln


def fact_node_migration(root: Path = ROOT):
    """Return ('Node migration: COMPLETE' | 'IN PROGRESS (n/4)', evidence)."""
    checks = []
    for rel, what in NODE_SOURCES:
        full = root / rel
        if what == "exists":
            checks.append((rel, _exists(full)))
        elif what == "findNode and no findGitBash":
            body = _read(full)
            checks.append((rel + " findNode & no findGitBash",
                           "findNode" in body and "findGitBash" not in body))
        else:
            body = _read(full)
            checks.append((rel + " " + what, what in body))
    ok = sum(1 for _, c in checks if c)
    if ok == len(checks):
        return ("Node migration: COMPLETE",
                "; ".join(n for n, _ in checks) + " -- all satisfied")
    missing = [n for n, c in checks if not c]
    return ("Node migration: IN PROGRESS (%d/%d)" % (ok, len(checks)),
            "missing: " + ", ".join(missing))


def fact_unicode_utf8(root: Path = ROOT):
    """Return ('Unicode UTF-8 widening: COMPLETE' | 'IN PROGRESS (n/4)', evidence)."""
    checks = []
    for rel, sym in UNICODE_SITES:
        body = _read(root / rel)
        checks.append((rel + " contains " + sym, sym in body))
    ok = sum(1 for _, c in checks if c)
    if ok == len(checks):
        return ("Unicode UTF-8 widening: COMPLETE",
                "; ".join(n for n, _ in checks) + " -- all satisfied")
    missing = [n for n, c in checks if not c]
    return ("Unicode UTF-8 widening: IN PROGRESS (%d/%d)" % (ok, len(checks)),
            "missing: " + ", ".join(missing))


def build_section(root: Path = ROOT, matrix_path: Path = None) -> str:
    """Build the full marker block (markers + section body)."""
    lines = [OPEN_MARK, "", SECTION_TITLE, ""]
    for status, evidence in (fact_closure(root, matrix_path),
                             fact_node_migration(root),
                             fact_unicode_utf8(root)):
        lines.append("- **" + status + "**")
        lines.append("  - 证据：" + evidence)
    lines.append("")
    lines.append(CLOSE_MARK)
    return "\n".join(lines) + "\n"


def _locate_block(text: str):
    """Return (start, end) of the marker block (inclusive markers) or None."""
    i = text.find(OPEN_MARK)
    if i < 0:
        return None
    j = text.find(CLOSE_MARK, i)
    if j < 0:
        return None
    return i, j + len(CLOSE_MARK)


def _block_extent(text: str):
    """Like _locate_block but also consumes the block's own trailing newline,
    so update/check treat exactly the same byte span (idempotent by design)."""
    loc = _locate_block(text)
    if loc is None:
        return None
    start, end = loc
    if end < len(text) and text[end] == "\n":
        end += 1
    return start, end


def update_doc(plan_path: Path, root: Path = ROOT, matrix_path: Path = None) -> str:
    """Write the fact block into the plan doc; manual sections untouched."""
    block = build_section(root, matrix_path)
    if not plan_path.exists():
        raise FileNotFoundError("plan doc missing: %s" % plan_path)
    text = plan_path.read_text(encoding="utf-8")
    loc = _block_extent(text)
    if loc is not None:
        new_text = text[:loc[0]] + block + text[loc[1]:]
    else:
        # Insert before the first '## ' heading (facts first, after the intro).
        m = re.search(r"^## ", text, re.M)
        if m:
            new_text = text[:m.start()] + block + text[m.start():]
        else:
            new_text = text + ("\n" if not text.endswith("\n") else "") + block
    plan_path.write_text(new_text, encoding="utf-8", newline="\n")
    return new_text


def check_doc(plan_path: Path, root: Path = ROOT, matrix_path: Path = None):
    """Compare the on-disk block with the freshly computed one (byte-exact)."""
    if not plan_path.exists():
        return False, "plan doc missing: %s (run the generator)" % plan_path
    text = plan_path.read_text(encoding="utf-8")
    loc = _block_extent(text)
    if loc is None:
        return False, ("plan doc lacks the marker block (%s .. %s): "
                       "run python scripts/generate_plan_status.py" % (OPEN_MARK, CLOSE_MARK))
    existing = text[loc[0]:loc[1]]
    expected = build_section(root, matrix_path)
    if existing == expected:
        return True, "plan fact block is up-to-date"
    diff = "\n".join(difflib.unified_diff(
        existing.splitlines(), expected.splitlines(),
        "plan doc block", "generated block", lineterm=""))
    return False, "plan fact block is stale:\n" + diff


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate/verify the repo-fact section of docs/plans/2026-09-03-032")
    parser.add_argument("--check", action="store_true",
                        help="verify the on-disk fact block matches the facts (exit 1 on drift)")
    args = parser.parse_args()

    if args.check:
        ok, detail = check_doc(PLAN_PATH, ROOT)
        if ok:
            print("[OK] " + detail)
            sys.exit(0)
        print("[ERROR] " + detail, file=sys.stderr)
        sys.exit(1)

    new_text = update_doc(PLAN_PATH, ROOT)
    print("[OK] refreshed plan fact block in %s (%d lines total; manual sections untouched)"
          % (PLAN_PATH, len(new_text.splitlines())))


if __name__ == "__main__":
    main()
