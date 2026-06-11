#!/usr/bin/env python3
"""Coupling count script -- reports cross-module #include counts per engine module."""
import re, sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC  = ROOT / "src"
MODS = sorted(d.name for d in SRC.iterdir() if d.is_dir() and d.name != "Platform")
THRESHOLDS = {"Core": 9, "Scripting": 5}
INCLUDE_RE = re.compile(r'^#include\s+"([^"]+)"')

def classify(ipath):
    for p in ipath.replace("\\", "/").split("/"):
        if p in MODS: return p
    return None

def count(mdir):
    cross = defaultdict(int)
    for f in list(mdir.rglob("*.cpp")) + list(mdir.rglob("*.h")):
        try:
            for line in f.read_text(encoding="utf-8", errors="replace").splitlines():
                m = INCLUDE_RE.match(line.strip())
                if m:
                    t = classify(m.group(1))
                    if t and t != mdir.name: cross[t] += 1
        except: pass
    return dict(cross)

def main():
    ci = "--ci" in sys.argv; ec = 0
    print("Cross-module #include counts:\n" + "-" * 55)
    for m in MODS:
        c = count(SRC / m)
        n, tot = len(c), sum(c.values())
        details = ", ".join(f"{k}:{v}" for k, v in sorted(c.items()))
        flag = ""
        if m in THRESHOLDS and n > THRESHOLDS[m]:
            flag = f"  *** EXCEEDS {THRESHOLDS[m]}"; ec = 1
        print(f"  {m:<12} -> {n:>2} modules ({tot:>3} total)  {details}{flag}")
    print("-" * 55)
    if ci and ec:
        print("\nFAIL: Coupling thresholds exceeded."); sys.exit(1)
    elif ci: print("\nPASS: All modules within thresholds.")
    else: print("\nDone.")

if __name__ == "__main__": main()
