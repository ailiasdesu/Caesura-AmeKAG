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

    if problems:
        print(f"TEST COVERAGE: {len(problems)} problem(s) found:")
        for p in problems:
            print("  -", p)
        print("New tests must be registered in the runner / CMakeLists to run.")
        sys.exit(1)
    print(f"TEST COVERAGE OK: {len(lua_tests)} lua + {len(cpp_tests)} cpp tests all registered.")


if __name__ == "__main__":
    main()
