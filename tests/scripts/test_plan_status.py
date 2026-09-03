#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_plan_status.py — t196 contract tests for scripts/generate_plan_status.py.

Covers: closure-count parsing (incl. EXPERIMENTAL(人工) label + missing matrix),
Node-migration tri-state (COMPLETE / IN PROGRESS (n/4) with missing list / the
findGitBash backslide), Unicode four-site tri-state, write+check round trip
with manual sections untouched, idempotency, and the missing-marker stale path.

Run directly:  python tests/scripts/test_plan_status.py
"""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "scripts"))

import generate_plan_status as gps  # noqa: E402

MATRIX = (
    "# Capability Closure Matrix (auto-generated)\n\n"
    "- UNWIRED：0 · PARTIAL：3 · CLOSED：127 · EXTRA：31 · EXPERIMENTAL(人工)：4\n\n"
    "---\n"
)


class TestPlanStatus(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name)
        self.matrix = self.root / "docs" / "design" / "capability-closure-matrix.md"
        self.plan = self.root / "docs" / "plans" / "plan.md"
        (self.root / "docs" / "plans").mkdir(parents=True)
        (self.root / "docs" / "design").mkdir(parents=True)

    def test_a_count_parse_including_experimental_label(self):
        self.matrix.write_text(MATRIX, encoding="utf-8")
        st, ev = gps.fact_closure(self.root)
        self.assertEqual(st,
                         "closure: PARTIAL=3 / CLOSED=127 / UNWIRED=0 / EXTRA=31 / EXPERIMENTAL=4",
                         st)
        self.assertIn("matrix.md:", ev)

    def test_a_count_missing_matrix(self):
        st, ev = gps.fact_closure(self.root)
        self.assertEqual(st, "closure: matrix MISSING (generator pending)")

    def test_b_node_complete_then_progress_and_backslide(self):
        (self.root / "scripts").mkdir(parents=True)
        (self.root / "scripts/package_game.mjs").write_text("x", encoding="utf-8")
        (self.root / "src/rpc/services").mkdir(parents=True)
        (self.root / "src/rpc/services/PackagingService.cpp").write_text(
            "findNode(x)", encoding="utf-8")
        (self.root / "scripts/caesura_build.py").write_text("find_node()", encoding="utf-8")
        (self.root / "scripts/package_distribution.py").write_text(
            "_find_node()", encoding="utf-8")
        st, ev = gps.fact_node_migration(self.root)
        self.assertEqual(st, "Node migration: COMPLETE", st)
        # drop one site -> IN PROGRESS (3/4) with the missing item listed
        (self.root / "scripts/caesura_build.py").unlink()
        st, ev = gps.fact_node_migration(self.root)
        self.assertEqual(st, "Node migration: IN PROGRESS (3/4)", st)
        self.assertIn("scripts/caesura_build.py find_node", ev)
        # findGitBash backslide -> the cpp condition fails (2/4)
        (self.root / "src/rpc/services/PackagingService.cpp").write_text(
            "findNode(x); findGitBash()", encoding="utf-8")
        st, ev = gps.fact_node_migration(self.root)
        self.assertEqual(st, "Node migration: IN PROGRESS (2/4)", st)
        self.assertIn("PackagingService.cpp findNode & no findGitBash", ev)

    def test_c_unicode_complete_then_progress(self):
        for rel, sym in gps.UNICODE_SITES:
            f = self.root / rel
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text(sym + "()", encoding="utf-8")
        st, ev = gps.fact_unicode_utf8(self.root)
        self.assertEqual(st, "Unicode UTF-8 widening: COMPLETE", st)
        (self.root / gps.UNICODE_SITES[1][0]).unlink()
        st, ev = gps.fact_unicode_utf8(self.root)
        self.assertEqual(st, "Unicode UTF-8 widening: IN PROGRESS (3/4)", st)
        self.assertIn("lua.c contains wmain", ev)

    def test_d_write_check_idempotent_manual_untouched(self):
        manual = "# plan\n\nIntro.\n\n## 1. manual section\nkeep me\n"
        self.plan.write_text(manual, encoding="utf-8")
        gps.update_doc(self.plan, self.root)
        out = self.plan.read_text(encoding="utf-8")
        self.assertIn(gps.OPEN_MARK, out)
        self.assertIn(gps.CLOSE_MARK, out)
        self.assertIn("keep me", out, "manual section must be untouched")
        self.assertIn("## 1. manual section", out, "manual heading intact")
        # fact block sits before the manual section (inserted before first heading)
        self.assertGreater(out.index("keep me"), out.index(gps.OPEN_MARK))
        # idempotency: second write leaves the file byte-identical
        before = self.plan.read_text(encoding="utf-8")
        gps.update_doc(self.plan, self.root)
        self.assertEqual(before, self.plan.read_text(encoding="utf-8"),
                         "update_doc must be idempotent")
        # positive check
        ok, detail = gps.check_doc(self.plan, self.root)
        self.assertTrue(ok, detail)
        # negative check (tamper the block only -- the section title is a
        # stable anchor present in every generated block)
        tampered = self.plan.read_text(encoding="utf-8").replace(
            "## 事实状态（自动生成 — 勿手改）", "## 事实状态X（自动生成 — 勿手改）")
        self.plan.write_text(tampered, encoding="utf-8")
        ok, detail = gps.check_doc(self.plan, self.root)
        self.assertFalse(ok)
        self.assertIn("stale", detail)

    def test_e_missing_marker_is_stale(self):
        self.plan.write_text("# plan\n\nno facts here\n", encoding="utf-8")
        ok, detail = gps.check_doc(self.plan, self.root)
        self.assertFalse(ok)
        self.assertIn("lacks the marker block", detail)


if __name__ == "__main__":
    unittest.main(verbosity=2)
