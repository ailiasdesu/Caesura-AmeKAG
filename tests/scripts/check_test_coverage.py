#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_test_coverage.py — prevent orphan-test regressions (round 14).

Verifies that every test_*.lua in tests/scripts/ is registered in the main
runner (run_lua_tests.lua) OR the isolated orphan runner (run_orphan_tests.lua),
and that every test_*.cpp in tests/cpp/ is registered in tests/CMakeLists.txt.

Exit code 1 lists any unregistered test — the "silent green" failure mode.
Usage: python tests/scripts/check_test_coverage.py
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPTS = os.path.join(ROOT, "tests", "scripts")
CPP = os.path.join(ROOT, "tests", "cpp")
CMAKE = os.path.join(ROOT, "tests", "CMakeLists.txt")


def read(p):
    with io.open(p, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def main():
    problems = []

    # --- Lua side ---------------------------------------------------------
    lua_tests = sorted(
        fn[:-4] for fn in os.listdir(SCRIPTS)
        if fn.startswith("test_") and fn.endswith(".lua")
    )
    registered = set()
    for runner in ("run_lua_tests.lua", "run_orphan_tests.lua"):
        src = read(os.path.join(SCRIPTS, runner))
        registered |= set(re.findall(r'"([a-z0-9_]+)"', src))
    # "replay" is a preload require, not a test — exclude it from the check.
    registered.discard("replay")
    for t in lua_tests:
        if t not in registered:
            problems.append(f"Lua test not registered: tests/scripts/{t}.lua")

    # --- C++ side ---------------------------------------------------------
    cpp_tests = sorted(
        fn for fn in os.listdir(CPP)
        if fn.startswith("test_") and fn.endswith(".cpp")
    )
    cmake = read(CMAKE)
    listed = set(re.findall(r"cpp/(test_[a-z0-9_]+.cpp)", cmake))
    for f in cpp_tests:
        if f not in listed:
            problems.append(f"C++ test not in CMakeLists file list: tests/cpp/{f}")

    # --- Editor command-highlight drift ------------------------------------
    # The editor's Monaco KAG_COMMANDS table must cover every schema contract
    # command; a missing command renders it as tag.invalid (round 19).
    editor_lang = os.path.join(ROOT, "editor", "src", "ide", "kagLanguage.ts")
    doc = os.path.join(ROOT, "docs", "api", "command-contracts.md")
    if os.path.exists(editor_lang) and os.path.exists(doc):
        lang_src = read(editor_lang)
        m = re.search(r"KAG_COMMANDS" + chr(92) + "s*=" + chr(92) + "s*" + chr(92) + "[(.*?)" + chr(92) + "]", lang_src, re.S)
        editor_cmds = set(re.findall(chr(39) + "([a-z0-9_]+)" + chr(39), m.group(1))) if m else set()
        doc_src = read(doc)
        doc_cmds = set(re.findall("^### " + chr(96) + chr(92) + "[([a-z0-9_]+)" + chr(92) + "]", doc_src, re.M))
        for c in sorted(doc_cmds):
            if c not in editor_cmds:
                problems.append(f"Editor highlight missing schema command: {c}")
        # TEMP-DEBUG

    if problems:
        print(f"TEST COVERAGE: {len(problems)} problem(s) found:")
        for p in problems:
            print("  -", p)
        print("New tests must be registered in the runner / CMakeLists to run.")
        sys.exit(1)
    print(f"TEST COVERAGE OK: {len(lua_tests)} lua + {len(cpp_tests)} cpp tests all registered.")


if __name__ == "__main__":
    main()