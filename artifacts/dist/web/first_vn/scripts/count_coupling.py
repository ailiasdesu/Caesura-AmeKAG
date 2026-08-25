#!/usr/bin/env python3
"""Report and enforce cross-module include counts and API boundaries."""
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC  = ROOT / "src"
MODS = sorted(d.name for d in SRC.iterdir() if d.is_dir())
ELEVATED_THRESHOLDS = {"entry": 14, "di": 14, "script": 14}
THRESHOLDS = {module: ELEVATED_THRESHOLDS.get(module, 4) for module in MODS}
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
SOURCE_PATTERNS = ("*.cpp", "*.cc", "*.cxx", "*.h", "*.hpp")
ALLOWED_NON_API_INCLUDES = {
    ("di", "BackendRegistry.h"),
    ("debug", "DebugManager.h"),
}

def classify(ipath):
    for p in ipath.replace("\\", "/").split("/"):
        if p in MODS: return p
    return None

def is_allowed_cross_include(source_module, target_module, include_path):
    if source_module == "entry":
        return True

    parts = include_path.replace("\\", "/").split("/")
    target_index = parts.index(target_module)
    target_path = "/".join(parts[target_index + 1:])
    if target_path.startswith("api/"):
        return True
    return (target_module, target_path) in ALLOWED_NON_API_INCLUDES

def count(mdir):
    cross = defaultdict(int)
    violations = []
    source_files = [f for pattern in SOURCE_PATTERNS for f in mdir.rglob(pattern)]
    for f in source_files:
        for line_number, line in enumerate(
                f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            m = INCLUDE_RE.match(line.strip())
            if m:
                include_path = m.group(1)
                t = classify(include_path)
                if t and t != mdir.name:
                    cross[t] += 1
                    if not is_allowed_cross_include(mdir.name, t, include_path):
                        violations.append((f, line_number, include_path))
    return dict(cross), violations

def main():
    ci = "--ci" in sys.argv; ec = 0
    print("Cross-module #include counts:\n" + "-" * 55)
    for m in MODS:
        c, violations = count(SRC / m)
        n, tot = len(c), sum(c.values())
        threshold = THRESHOLDS[m]
        details = ", ".join(f"{k}:{v}" for k, v in sorted(c.items()))
        flag = ""
        if n > threshold:
            flag = f"  *** EXCEEDS {threshold}"
            ec = 1
        if violations:
            flag += f"  *** {len(violations)} NON-API INCLUDE(S)"
            ec = 1
        print(
            f"  {m:<12} -> {n:>2}/{threshold:<2} modules "
            f"({tot:>3} total)  {details}{flag}"
        )
        for path, line_number, include_path in violations:
            relative_path = path.relative_to(ROOT).as_posix()
            print(f"      {relative_path}:{line_number}: {include_path}")
    print("-" * 55)
    if ci and ec:
        print("\nFAIL: Coupling or API-boundary rules exceeded."); sys.exit(1)
    elif ci: print("\nPASS: All modules within thresholds and API boundaries.")
    else: print("\nDone.")

if __name__ == "__main__": main()
