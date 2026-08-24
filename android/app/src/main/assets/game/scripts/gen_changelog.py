#!/usr/bin/env python3
"""Generate CHANGELOG.md from conventional commits in the git history.

Pure Python standard library: no third-party dependencies, runs on Windows
and in git bash alike. It parses `git log --format=%s` (subject line of each
commit) and groups commits by Conventional-Commits type
(feat/fix/test/docs/...), emitting a Markdown changelog to the repository root.

Usage:
    python scripts/gen_changelog.py                 # last 100 commits -> CHANGELOG.md
    python scripts/gen_changelog.py --count 50      # last 50 commits
    python scripts/gen_changelog.py --since 2026-08-01
    python scripts/gen_changelog.py --from-tag v1.0.0-alpha
    python scripts/gen_changelog.py --tag v0.1.0    # header version for this segment
    python scripts/gen_changelog.py --dry-run       # print to stdout instead of writing

Header/section behaviour
------------------------
* By default the output is a single "Unreleased" section listing the parsed
  commits of the selected range, newest first, grouped by type.
* Pass `--tag <name>` to title the section "<name>" instead of "Unreleased".
* Pass `--from-tag <tag>` to limit the range to commits after a tag.

Type ordering is fixed (feat, fix, perf, refactor, test, docs, build, ci,
chore, merge, plan, review, other); every commit is classified by its leading
`type(scope):` token, and the scope is preserved alongside the subject.
"""
import argparse
import re
import subprocess
import sys
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "CHANGELOG.md"

TYPE_ORDER = [
    "feat", "fix", "perf", "refactor", "test", "docs",
    "build", "ci", "chore", "merge", "plan", "review", "other",
]

# Conventional-Commit type token at the start of a subject line.
TOKEN_RE = re.compile(r"^([a-z][a-z0-9]*)(?:\s*\((.*?)\))?:\s*(.*)$")

TITLE = "Caesura (AmeKAG)"


def run_git(args):
    """Run a git command, returning decoded stdout or raising on failure."""
    proc = subprocess.run(
        ["git"] + args,
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if proc.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (" ".join(args), proc.stderr.strip()))
    return proc.stdout


def gather_commits(args):
    """Return a list of dicts {sha, date, subject} for the selected range."""
    if args.from_tag:
        log_args = ["log", "{0}..HEAD".format(args.from_tag),
                    "--pretty=format:%h|%ad|%s", "--date=short"]
    elif args.since:
        # git treats a bare date's midnight in a way that can miss same-day
        # commits depending on the commit timezone; pin an explicit time so
        # "--since 2026-08-15" is inclusive of that whole day.
        since = args.since
        if re.match(r"^\d{4}-\d{2}-\d{2}$", since):
            since += " 00:00:00"
        log_args = ["log", "--since={0}".format(since),
                    "--pretty=format:%h|%ad|%s", "--date=short"]
    else:
        count = args.count if args.count is not None else 100
        log_args = ["log", "-n", str(count),
                    "--pretty=format:%h|%ad|%s", "--date=short"]
    log_args.append("--")
    text = run_git(log_args)
    commits = []
    for line in text.splitlines():
        if not line:
            continue
        sha, date, subj = line.split("|", 2)
        commits.append({"sha": sha, "date": date, "subject": subj})
    return commits


def format_subject(subject):
    """Return a clean one-line description: capitalise, and drop verbose
    multi-clause tails that follow an em dash (U+2014), keeping the concise
    conventional-commit summary. Inline literals (e.g. `[i18n lang=]`) are
    left intact since they carry command syntax."""  
    subject = subject.strip()
    if not subject:
        return subject
    # Cut at the em dash (" \u2014 ") used to separate title from explanation.
    idx = subject.find(" \u2014 ")
    if idx != -1:
        subject = subject[:idx].rstrip()
    return subject[0].upper() + subject[1:]


def classify(subject):
    """Split a subject into (type, scope, desc); falls back to 'other'."""
    m = TOKEN_RE.match(subject)
    if not m:
        return "other", "", format_subject(subject)
    typ, scope, desc = m.group(1), (m.group(2) or ""), m.group(3)
    if typ not in TYPE_ORDER:
        return "other", "", format_subject(subject)
    return typ, scope, format_subject(desc)


def group_by_type(commits):
    """Group commits into an ordered {type: [entry]} mapping."""
    grouped = OrderedDict((t, []) for t in TYPE_ORDER)
    for c in commits:
        typ, scope, desc = classify(c["subject"])
        grouped[typ].append({"scope": scope, "desc": desc, "sha": c["sha"]})
    return grouped


def render_section(title, date, commits):
    """Render one changelog section (a version or date segment)."""
    grouped = group_by_type(commits)
    non_empty = [(t, g) for t, g in grouped.items() if g]
    lines = ["## {0}".format(title), ""]
    if date:
        lines.append("*{0}*".format(date))
        lines.append("")
    if not non_empty:
        lines.append("_No conventional commits parsed in this range._")
        lines.append("")
        return lines
    for typ, entries in non_empty:
        lines.append("### {0}".format(typ.capitalize()))
        lines.append("")
        for e in entries:
            prefix = "- **{0}** ".format(e["scope"]) if e["scope"] else "- "
            lines.append("{0}{1} (`{2}`)".format(prefix, e["desc"], e["sha"]))
        lines.append("")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--count", type=int, default=None, help="Last N commits (default 100)")
    ap.add_argument("--since", default=None, help="YYYY-MM-DD: only commits from this date")
    ap.add_argument("--from-tag", default=None, help="Only commits after this tag (..HEAD)")
    ap.add_argument("--tag", default=None, help="Section title (default 'Unreleased')")
    ap.add_argument("--dry-run", action="store_true", help="Print to stdout, don't write")
    args = ap.parse_args()

    commits = gather_commits(args)
    if not commits:
        sys.stderr.write("No commits matched the requested range.\n")
        return 1

    lines = ["# {0} — Changelog".format(TITLE), ""]
    lines.append("This file is generated from Conventional Commits. Regenerate it with "
                 "`python scripts/gen_changelog.py`.")
    lines.append("")

    title = args.tag or "Unreleased"
    lines.extend(render_section(title, commits[0]["date"], commits))

    body = "\n".join(lines).rstrip() + "\n"

    if args.dry_run:
        sys.stdout.write(body)
        return 0

    OUTPUT.write_text(body, encoding="utf-8")
    print("Wrote {0} commits to {1}".format(len(commits), OUTPUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
