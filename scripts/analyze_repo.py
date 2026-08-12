#!/usr/bin/env python3
"""Repo cleanup analysis: duplicate docs, orphan scripts, unused references.
Read-only analysis; prints findings. Run from repo root."""
import os
import re
import hashlib

ROOT = os.getcwd()


def sha1(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def rel(p):
    return os.path.relpath(p, ROOT).replace("\\", "/")


def main():
    print("=== 1. 完全重复文件（git 跟踪，非 external）===")
    by_hash = {}
    for root, dirs, fs in os.walk("."):
        if any(x in root for x in (".git", "external", "build", "build_", "build-", ".vs", "bin", "thirdparty")):
            continue
        for f in fs:
            p = os.path.join(root, f)
            try:
                h = sha1(p)
            except Exception:
                continue
            by_hash.setdefault(h, []).append(p)
    dupes = {h: v for h, v in by_hash.items() if len(v) > 1}
    if not dupes:
        print("  (无)")
    for h, v in dupes.items():
        print(f"  {len(v)} 份: " + " | ".join(rel(p) for p in v))

    print("\n=== 2. docs/ 近重复（前 100 行 Jaccard > 0.5）===")
    doc_lines = {}
    for root, dirs, fs in os.walk("docs"):
        for f in fs:
            if f.endswith(".md"):
                p = os.path.join(root, f)
                try:
                    lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
                except Exception:
                    continue
                doc_lines[rel(p)] = set(l.strip() for l in lines[:100] if l.strip())
    keys = list(doc_lines)
    for i in range(len(keys)):
        for j in range(i + 1, len(keys)):
            a, b = keys[i], keys[j]
            sa, sb = doc_lines[a], doc_lines[b]
            if not sa or not sb:
                continue
            inter = len(sa & sb)
            union = len(sa | sb)
            if union and inter / union > 0.5:
                print(f"  {a} <-> {b} (前100行相似度 {inter/union:.2f})")

    print("\n=== 3. 孤儿检测 ===")
    # scripts/ files never referenced by any tracked file
    scripts = set()
    for root, dirs, fs in os.walk("scripts"):
        for f in fs:
            if f.endswith(".lua"):
                scripts.add(rel(os.path.join(root, f)))
    referenced = set()
    tracked = [l.strip() for l in os.popen("git ls-files").read().splitlines()]
    for f in tracked:
        try:
            text = open(f, encoding="utf-8", errors="replace").read()
        except Exception:
            continue
        for m in re.finditer(r"[\"'](scripts/[A-Za-z0-9_./-]+\.lua)[\"']", text):
            referenced.add(m.group(1))
    orphans = sorted(s for s in scripts if s not in referenced)
    print(f"  scripts/ 共 {len(scripts)} 个，无引用 {len(orphans)}:")
    for o in orphans:
        print(f"    {o}")

    print("\n=== 4. docs/ 无人引用的文档（不被任何 .md 链接）===")
    md_files = [rel(p) for p in by_hash and []]
    all_docs = []
    for root, dirs, fs in os.walk("docs"):
        for f in fs:
            if f.endswith(".md"):
                all_docs.append(rel(os.path.join(root, f)))
    linked = set()
    for f in tracked:
        if not f.endswith(".md"):
            continue
        try:
            text = open(f, encoding="utf-8", errors="replace").read()
        except Exception:
            continue
        for m in re.finditer(r"\]\(([^)]+\.md)(?:#[^)]*)?\)", text):
            target = m.group(1).split("#")[0]
            linked.add(target)
    unreferenced = []
    for d in all_docs:
        name = d.split("/")[-1]
        found = False
        for l in linked:
            if l.endswith(name) or l == d or l.replace("docs/", "") == d.replace("docs/", ""):
                found = True
                break
        if not found:
            unreferenced.append(d)
    print(f"  docs/ 共 {len(all_docs)} 个文档，未被其他文档链接 {len(unreferenced)}:")
    for d in unreferenced:
        print(f"    {d}")

    print("\n=== 5. 顶层杂项 ===")
    # Legacy top-level files that were removed in repo cleanup (2026-08):
    # STRATEGY.md (stale, superseded by README/AGENTS/CLAUDE). If any of these
    # reappear, flag them.
    for f in []:
        if os.path.exists(f):
            print(f"  {f}: {os.path.getsize(f)} bytes")
    # .claude / .superpowers tracked?
    st = os.popen("git ls-files .claude .superpowers .reasonix").read().strip()
    print(f"  .claude/.superpowers/.reasonix 跟踪文件: {len(st.splitlines()) if st else 0}")


if __name__ == "__main__":
    main()
