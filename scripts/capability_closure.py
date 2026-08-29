#!/usr/bin/env python3
"""Capability Closure scanner v2 (029 artifact A; t103 MUST-FIX + overrides).

For every KAG command contract in docs/api/command-contracts.md, grade the
four closure dimensions that can be measured statically:

  Declared   -- the command has a contract entry (docs/api/command-contracts.md)
  Dispatched -- a handler is registered into the KAG dispatch table
                (scripts/kag/commands/*.lua export tables + scripts/kag.lua
                explicit mappings + scripts/kag/sma.lua sma_commands)
  Consumed   -- the handler body CALLS an effect surface in a call context
                (backend.foo( / layers.foo( / kag.foo( / ctx.tf field/assign).
                V2 (t103): comments and string literals are stripped BEFORE
                the call scan, and a bare mention (no parenthesis call or
                field access) no longer counts -- the v1 substring heuristic
                produced false positives from require("kag.xxx") literals and
                trailing comment blocks (assert / endbutton / waitclick).
  Tested     -- HEURISTIC reference count in tests/scripts/*.lua and
                web/*.test.js (NOT assertion-level coverage)

Observable / Platform Tested / Packaged are manual placeholders (?) unless
overridden in docs/design/capability-closure-overrides.json (human layer).

Outputs:
  build/capability-closure.json             raw data
  docs/design/capability-closure-matrix.md  generated markdown (do not edit)

Idempotent: deterministic given unchanged inputs (generated_at = newest input
mtime; no runtime data in the output).

Usage:
  python scripts/capability_closure.py
"""

import json
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONTRACTS = REPO / "docs" / "api" / "command-contracts.md"
KAG_LUA = REPO / "scripts" / "kag.lua"
SMA_LUA = REPO / "scripts" / "kag" / "sma.lua"
COMMANDS_DIR = REPO / "scripts" / "kag" / "commands"
TESTS_LUA = REPO / "tests" / "scripts"
TESTS_WEB = REPO / "web"
OVERRIDES = REPO / "docs" / "design" / "capability-closure-overrides.json"
OUT_JSON = REPO / "build" / "capability-closure.json"
OUT_MD = REPO / "docs" / "design" / "capability-closure-matrix.md"

NL = chr(10)
TABLE_FILE = {
    "AudioCommands": "scripts/kag/commands/audio.lua",
    "CharacterCommands": "scripts/kag/commands/character.lua",
    "LayerCommands": "scripts/kag/commands/layer.lua",
    "LayoutCommands": "scripts/kag/commands/layout.lua",
    "MathCommands": "scripts/kag/commands/math.lua",
    "ResourceCommands": "scripts/kag/commands/resource.lua",
    "SaveCommands": "scripts/kag/commands/save.lua",
    "SystemCommands": "scripts/kag/commands/system.lua",
    "TextCommands": "scripts/kag/commands/text.lua",
    "TransCommands": "scripts/kag/commands/transition.lua",
    "TweenCommands": "scripts/kag/commands/tween.lua",
    "VFXCommands": "scripts/kag/commands/vfx.lua",
    "VideoCommands": "scripts/kag/commands/video.lua",
    "sma_commands": "scripts/kag/sma.lua",
}

# t103 MUST-FIX 1 regression locks: these commands must grade PARTIAL under
# the v2 call-context criteria because their bodies only use local modules
# (kag.expr / palette / state) -- v1 flagged them CLOSED via comment-block and
# string-literal contamination. If the code later gains a real effect call,
# the guard must be updated deliberately (t103: stay honest, never widen the
# heuristic to preserve a number).
GUARD_PARTIAL = {
    "assert": "body: kag.expr evaluate + error (no effect surface call)",
    "endbutton": "body: kag.expr evaluateTranslated + ctx state (no effect surface call)",
}


def source_mtime(paths):
    return max(int(p.stat().st_mtime) for p in paths if p.exists())


# ---------------------------------------------------------------------------
# Lua comment/string stripper (char state machine; no regex). Everything that
# is not code becomes a space so call-context scanning cannot see comments or
# string literals (require("kag.xxx") used to count as a kag. mention).
# ---------------------------------------------------------------------------


def strip_lua(code):
    out = []
    i = 0
    n = len(code)
    while i < n:
        c = code[i]
        nxt = code[i + 1] if i + 1 < n else ""
        if c == "-" and nxt == "-":
            # line comment, or long-bracket comment --[[ .. ]] / --[=[ .. ]=]
            j = i + 2
            level = 0
            if j < n and code[j] == "[":
                k = j + 1
                while k < n and code[k] == "=":
                    level += 1
                    k += 1
                if k < n and code[k] == "[":
                    close = "]" + ("=" * level) + "]"
                    e = code.find(close, k + 1)
                    e = e + len(close) if e >= 0 else n
                    out.extend([" "] * (e - i))
                    i = e
                    continue
            e = code.find(chr(10), i)
            e = e if e >= 0 else n
            out.extend([" "] * (e - i))
            i = e
        elif c == '"' or c == "'":
            j = i + 1
            while j < n:
                if code[j] == chr(92):
                    j += 2
                    continue
                if code[j] == c or code[j] == chr(10):
                    break
                j += 1
            e = (j + 1) if j < n else n
            out.extend([" "] * (e - i))
            i = e
        elif c == "[" and nxt == "[":
            j = i + 2
            level = 0
            while j < n and code[j] == "=":
                level += 1
                j += 1
            if j < n and code[j] == "[":
                close = "]" + ("=" * level) + "]"
                e = code.find(close, j + 1)
                e = e + len(close) if e >= 0 else n
                out.extend([" "] * (e - i))
                i = e
                continue
            out.append(c)
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def has_call(clean, tok):
    """True when an effect token appears in CALL context: tok.<ident>( ."""
    pos = 0
    n = len(clean)
    while True:
        p = clean.find(tok, pos)
        if p < 0:
            return False
        q = p + len(tok)
        s = q
        while s < n and (clean[s].isalnum() or clean[s] == "_"):
            s += 1
        if s == q:
            pos = p + 1
            continue
        while s < n and clean[s] in (" ", chr(9)):
            s += 1
        if s < n and clean[s] == "(":
            return True
        pos = p + 1


def has_ctx_tf_use(clean):
    """ctx.tf field access or assignment (ctx.tf. / ctx.tf[ / ctx.tf =)."""
    pos = 0
    while True:
        p = clean.find("ctx.tf", pos)
        if p < 0:
            return False
        q = p + len("ctx.tf")
        if q < len(clean) and clean[q] in ".=[":
            return True
        pos = p + 1


def call_hits(code):
    """Effect-surface call-context hits for a raw code slice (strips first)."""
    clean = strip_lua(code)
    hits = []
    for tok in ("backend.", "layers.", "kag."):
        if has_call(clean, tok):
            hits.append(tok)
    if has_ctx_tf_use(clean):
        hits.append("ctx.tf")
    return hits


def parse_contracts():
    names = []
    for line in open(CONTRACTS, encoding="utf-8").read().splitlines():
        if line.startswith("### `[") and line.endswith("]`"):
            names.append(line[6:-2])
    return names


def split_def(line, table):
    prefix = "function " + table + "."
    if not line.startswith(prefix):
        return None
    rest = line[len(prefix):]
    name = rest.split("(")[0].strip()
    return name


def full_path(file):
    """REPO-relative full evidence path (t103 MUST-FIX 2)."""
    if file == "kag.lua":
        return "scripts/kag.lua"
    if file == "kag/sma.lua":
        return "scripts/kag/sma.lua"
    return "scripts/kag/commands/" + file




def collect_subtable_names():
    """Table-like names in command files (name N with 'function N.' defs),
    excluding per-file export tables. Used to tell C:subtable-key assigns
    (TransCommands.Bezier = Bezier) from B:api-helper-export ones.
    """
    names = set()
    for f in sorted(COMMANDS_DIR.glob("*.lua")):
        txt = f.read_text(encoding="utf-8")
        for line in txt.splitlines():
            if line.startswith("function ") and "." in line:
                rest = line[len("function "):]
                nm = rest.split(".")[0].strip()
                if nm and nm.isidentifier():
                    names.add(nm)
    return names

def parse_command_modules():
    handlers = {}
    private = []
    for f in sorted(COMMANDS_DIR.glob("*.lua")):
        txt = f.read_text(encoding="utf-8")
        lines = txt.splitlines()
        table = None
        for line in lines:
            if line.startswith("return "):
                possible = line[7:].strip()
                if possible and possible.isidentifier():
                    table = possible
        if not table:
            continue
        defs = [i for i, l in enumerate(lines) if split_def(l, table)]
        for k, i in enumerate(defs):
            name = split_def(lines[i], table)
            body_end = defs[k + 1] if k + 1 < len(defs) else len(lines)
            body = NL.join(lines[i:body_end])
            line_no = i + 1
            if name.startswith("_"):
                private.append((name, f.name, line_no))
                continue
            handlers[name] = {"file": full_path(f.name), "line": line_no,
                              "body": body, "kind": "def"}
        for i, l in enumerate(lines):
            if not l.startswith(table + "."):
                continue
            if l.split("(", 1)[0].strip().startswith("function"):
                continue
            rest = l[len(table) + 1:]
            name = rest.split("=", 1)[0].strip()
            if not name or not name.isidentifier() or name.startswith("_"):
                continue
            if "=" not in l or name in handlers:
                continue
            rhs = l.split("=", 1)[1].strip()
            handlers[name] = {"file": full_path(f.name), "line": i + 1,
                              "body": rhs, "kind": "assign"}
    return handlers, private


def parse_sma_commands():
    txt = SMA_LUA.read_text(encoding="utf-8")
    lines = txt.splitlines()
    handlers = {}
    defs = [i for i, l in enumerate(lines) if split_def(l, "sma_commands")]
    for k, i in enumerate(defs):
        name = split_def(lines[i], "sma_commands")
        body_end = defs[k + 1] if k + 1 < len(defs) else len(lines)
        body = NL.join(lines[i:body_end])
        handlers[name] = {"file": full_path("kag/sma.lua"), "line": i + 1,
                          "body": body, "kind": "def"}
    return handlers


def parse_kag_lua_mappings():
    txt = KAG_LUA.read_text(encoding="utf-8")
    lines = txt.splitlines()
    out = {}
    defs = {}
    for idx, line in enumerate(lines):
        m = None
        if line.startswith("function KAG.") and "(" in line:
            rest = line[len("function KAG."):]
            name = rest.split("(")[0].strip()
            if name and name.isidentifier():
                m = ("def", name)
        if m is None and "=" in line:
            if line.startswith("KAG["):
                rhs = line.split("=", 1)[1].strip()
                inner = line[4:].split("]", 1)[0].strip().strip("\"'")
                m = ("asg", inner, rhs)
            elif line.startswith("KAG."):
                rhs = line.split("=", 1)[1].strip()
                rest = line[4:].split("=")[0].strip()
                n = ""
                for ch in rest:
                    if ch.isalnum() or ch == "_":
                        n += ch
                    else:
                        break
                m = ("asg", n, rhs)
        if m is None:
            continue
        if m[0] == "def":
            name = m[1]
            if name.startswith("_"):
                continue
            body_lines = []
            j = idx
            while j < len(lines):
                l = lines[j]
                if j > idx and (l.startswith("function KAG.") or l.startswith("KAG.")
                                or l.startswith("local ") or l.startswith("end}")):
                    break
                body_lines.append(l)
                j += 1
            defs[name] = {"file": full_path("kag.lua"), "line": idx + 1,
                          "body": NL.join(body_lines), "kind": "def"}
        else:
            name, rhs = m[1], m[2]
            if name == "name" and rhs == "handler":
                continue
            out[name] = {"line": idx + 1, "rhs": rhs}
    return out, defs


SUBTABLE_NAMES = set()


def classify_source(file_, kind, rhs):
    """t103 三分层 EXTRA 类型：
    A=user-command-missing-contract（KAG.* 直接 API/别名；owner=contracts 生成链，入册与否=产品决策待议）
    B=api-helper-export（命令模块导出辅助/API 助手，非用户命令面）
    C=subtable-key（pairs() 注册的表引用真噪声，非命令面）。
    """
    if file_ == "scripts/kag.lua":
        return "A:user-command-missing-contract"
    if kind == "assign" and rhs and rhs.isidentifier() and rhs in SUBTABLE_NAMES:
        return "C:subtable-key"
    return "B:api-helper-export"


def resolve_consumed(name, index, depth):
    rec = index[name]
    if rec.get("consumed") is not None or depth > 8:
        return rec.get("consumed") or False
    if rec["kind"] == "def":
        hits = call_hits(rec["body"])
        rec["consumed"] = bool(hits)
        rec["hits"] = hits
        return rec["consumed"]
    rhs = rec["body"]
    if rhs.startswith("function"):
        hits = call_hits(rhs)
        rec["consumed"] = bool(hits)
        rec["hits"] = hits
        return rec["consumed"]
    rec["alias_rhs"] = rhs
    for branch in rhs.split(" or "):
        parts = branch.strip().split(".")
        if len(parts) == 2 and parts[1].isidentifier() and parts[0].isidentifier():
            table, target = parts
            if target in index and (table == "KAG" or table in TABLE_FILE):
                ok = resolve_consumed(target, index, depth + 1)
                rec["consumed"] = ok
                rec["alias_target"] = table + "." + target
                return ok
    hits = call_hits(rhs)
    rec["consumed"] = bool(hits)
    rec["hits"] = hits
    return rec["consumed"]


def grade_status(declared, dispatched, consumed):
    if declared and not dispatched:
        return "UNWIRED"
    if declared and not consumed:
        return "PARTIAL"
    if declared:
        return "CLOSED"
    if dispatched:
        return "EXTRA"
    return "NONE"


def count_in(p, name):
    txt = p.read_text(encoding="utf-8", errors="replace")
    n = 0
    needle = "[" + name
    kag_needle = "kag." + name
    pos = txt.find(needle)
    while pos >= 0:
        after = txt[pos + len(needle):pos + len(needle) + 1]
        if after in (" ", "]", "=", chr(92)) or after == "" or after == chr(10):
            n += 1
        pos = txt.find(needle, pos + 1)
    pos = txt.find(kag_needle)
    while pos >= 0:
        after = txt[pos + len(kag_needle):pos + len(kag_needle) + 1]
        if after in ("", ".", " ", "(", "[", chr(10)) or (after.isalnum() is False):
            n += 1
        pos = txt.find(kag_needle, pos + 1)
    return n


def tested_count(name):
    total = 0
    files = []
    for base, globber in ((TESTS_LUA, "lua"), (TESTS_WEB, "js")):
        if not base.is_dir():
            continue
        if base == TESTS_WEB:
            paths = sorted(base.glob("*.test.js"))
        else:
            paths = sorted(base.rglob("*.lua"))
        for p in paths:
            n = count_in(p, name)
            if n:
                total += n
                files.append(str(p.relative_to(REPO)))
    return total, files


def load_overrides(known_names):
    # Human evidence layer (t103 adopt): docs/design/capability-closure-overrides.json.
    # Unknown command keys are loudly rejected (exit non-zero) -- never silently ignored.
    if not OVERRIDES.exists():
        return {}, []
    data = json.loads(OVERRIDES.read_text(encoding="utf-8"))
    commands = data.get("commands", {})
    oos = data.get("out_of_scope", [])
    bad = [k for k in commands if k not in known_names]
    if bad:
        print("[closure] ERROR: overrides reference unknown commands: " + ", ".join(bad))
        sys.exit(2)
    return commands, oos


def build_records(overrides_map):
    declared_names = set(parse_contracts())
    handlers, private = parse_command_modules()
    handlers.update(parse_sma_commands())
    kag_map, kag_defs = parse_kag_lua_mappings()
    handlers.update(kag_defs)
    index = {}
    for name, info in handlers.items():
        index[name] = {"kind": info.get("kind", "def"),
                       "file": info["file"],
                       "line": info["line"],
                       "body": info.get("body", ""),
                       "consumed": None, "hits": [],
                       "rhs": info.get("body", "")}
    for name, mp in kag_map.items():
        index[name] = {"kind": "assign", "file": "scripts/kag.lua",
                       "line": mp["line"], "body": mp["rhs"],
                       "consumed": None, "hits": [], "rhs": mp["rhs"]}
    for name in sorted(index):
        resolve_consumed(name, index, 0)
    contract_lines = {}
    for ln, l in enumerate(open(CONTRACTS, encoding="utf-8").read().splitlines(), 1):
        if l.startswith("### `[") and l.endswith("]`"):
            contract_lines[l[6:-2]] = ln
    records = []
    for name in sorted(set(declared_names) | set(index)):
        declared = name in declared_names
        rec = index.get(name)
        if rec:
            dispatched, consumed = True, rec.get("consumed") or False
            hits = rec.get("hits") or []
            evidence = rec["file"] + ":" + str(rec["line"])
            alias = rec.get("alias_target")
            source_type = classify_source(rec["file"], rec["kind"], rec.get("rhs", ""))
        else:
            dispatched, consumed, hits = False, False, []
            evidence = None
            alias = None
            source_type = None
        ov = overrides_map.get(name)
        tcount, tfiles = tested_count(name)
        rec_out = {
            "name": name, "declared": declared, "dispatched": dispatched,
            "consumed": consumed, "consumed_hits": hits,
            "tested_count": tcount, "tested_files": tfiles,
            "status": grade_status(declared, dispatched, consumed),
            "evidence": evidence, "alias": alias,
            "contract_line": contract_lines.get(name),
            "source_type": source_type,
            "override": ov or None,
        }
        if ov and ov.get("status"):
            rec_out["status"] = ov["status"]
        records.append(rec_out)
    return records, private, len(declared_names)


def guard_partial(records):
    # t103 MUST-FIX 1 self-check: named commands must grade PARTIAL under the
    # v2 call-context criteria (keep the lock, stay honest; a real effect call
    # later requires a deliberate guard update).
    by_name = {r["name"]: r for r in records}
    bad = []
    for name, why in GUARD_PARTIAL.items():
        row = by_name.get(name)
        if not row or row["status"] != "PARTIAL":
            bad.append(name + " -> " + (row["status"] if row else "missing"))
    if bad:
        print("[closure] ERROR: t103 regression locks violated: " + ", ".join(bad))
        sys.exit(3)


def render_markdown(records, private, declared_total, oos, generated_at, fp):
    by_status = {}
    for r in records:
        by_status.setdefault(r["status"], []).append(r)
    L = []
    L.append("# Capability Closure Matrix (auto-generated)")
    L.append("")
    L.append("> 由 python scripts/capability_closure.py 生成；勿手动编辑。")
    L.append("> 生成时间（输入源最新 mtime）：" + generated_at)
    L.append("> 生成命令：python scripts/capability_closure.py")
    L.append("> 源指纹（输入内容 sha256 前 16 hex）：" + fp)
    L.append("> 输出确定性：同源指纹同字节（generated_at 为输入源最新 mtime；跨机 checkout 的 mtime 差异属 by-design，确定性以指纹为准）")
    L.append("")
    L.append("## 概述")
    L.append("")
    n_disp = sum(1 for r in records if r["dispatched"])
    n_cons = sum(1 for r in records if r["dispatched"] and r["consumed"])
    n_test = sum(1 for r in records if r["tested_count"] > 0)
    n_unw = len(by_status.get("UNWIRED", []))
    n_part = len(by_status.get("PARTIAL", []))
    n_closed = len(by_status.get("CLOSED", []))
    n_extra = len(by_status.get("EXTRA", []))
    L.append("- 合约总数（Declared，docs/api/command-contracts.md）：**" + str(declared_total) + "**")
    L.append("- 已注册（Dispatched）：**" + str(n_disp) + "**")
    L.append("- 触达效果面（Consumed，调用形上下文，v2）：**" + str(n_cons) + "**")
    L.append("- 测试引用（Tested，启发式计数）：**" + str(n_test) + "**")
    L.append("- UNWIRED：" + str(n_unw) + " · PARTIAL：" + str(n_part)
             + " · CLOSED：" + str(n_closed) + " · EXTRA：" + str(n_extra))
    L.append("- **恒等式：" + str(declared_total) + " = CLOSED(" + str(n_closed)
             + ") + PARTIAL(" + str(n_part) + ") + UNWIRED(" + str(n_unw) + ")；"
             + str(n_disp) + " = " + str(declared_total) + "(Declared) + EXTRA(" + str(n_extra) + ")**")
    L.append("")
    L.append("**范围声明（t103 MUST-FIX 3）**：本矩阵的 " + str(declared_total)
             + " = 声明式 KAG 命令合约闭包（docs/api/command-contracts.md 全量条目）。下列能力**不在 "
             + str(declared_total) + " 内**：")
    L.append("- 原生手势链：SwipeDown / SwipeUp / LongPress / Pinch / TwoFingerTap / ThreeFingerHold（平台层）；")
    L.append("- 文本标记参数：letter_spacing / spacing / font / line_height 等内联标记（非命令）；")
    L.append("- KAG.* Lua API：jump/call/return_to_caller 等直接 API（注册键存在但非合约命令，见 EXTRA 的 api-alias）。")
    L.append("这些能力由矩阵底部「人工判级（范围外能力）」区段承载（docs/design/capability-closure-overrides.json 驱动）。")
    L.append("**EXTRA 属设计行为**：contracts 由声明式 schema registry（kag/schema.lua，经 scripts/schema_doc.lua 生成）" )
    L.append("产出；流控/API 命令按设计不在注册表——EXTRA 不是缺陷信号（A 类入册与否=产品决策待议）。")
    L.append("")
    L.append("> 状态定义：**UNWIRED**=有合约无处理器；**PARTIAL**=已注册但处理器体未以调用形触达效果面（v2 口径）；")
    L.append("> **CLOSED**=已注册且调用形触达效果面；**EXTRA**=已注册但无合约。")
    L.append("> ⚠ = 人工覆盖（docs/design/capability-closure-overrides.json；详见『人工覆盖』节）。")
    L.append("")
    L.append("## Commands")
    L.append("")
    L.append("| Command | Declared | Dispatched | Consumed | Tested | Observable | Platform Tested | Packaged | Status | 证据 |")
    L.append("|---|---|---|---|---|---|---|---|---|---|")
    for r in sorted(records, key=lambda x: x["name"]):
        ov = r.get("override") or {}
        obs = str(ov.get("observable", "?"))
        pt = str(ov.get("platform_tested", "?"))
        pk = str(ov.get("packaged", "?"))
        mark = " ⚠" if ov else ""
        L.append("| " + r["name"] + " | "
                 + ("Y" if r["declared"] else "n") + " | "
                 + ("Y" if r["dispatched"] else "n") + " | "
                 + ("Y" if r["consumed"] else "n") + " | "
                 + (str(r["tested_count"]) if r["tested_count"] else "-") + " | "
                 + obs + mark + " | " + pt + mark + " | " + pk + mark + " | "
                 + r["status"] + " | " + (r["evidence"] or "-") + " |")
    L.append("")
    L.append("## 人工覆盖（⚠）")
    L.append("")
    ovs = [r for r in records if r.get("override")]
    if ovs:
        for r in sorted(ovs, key=lambda x: x["name"]):
            ov = r["override"]
            L.append("- " + r["name"] + " — Observable=" + str(ov.get("observable", "?"))
                     + " · PlatformTested=" + str(ov.get("platform_tested", "?"))
                     + " · Packaged=" + str(ov.get("packaged", "?"))
                     + ((" · Status=" + str(ov["status"])) if ov.get("status") else ""))
            L.append("  - reason：" + str(ov.get("reason", "")))
            L.append("  - evidence：" + str(ov.get("evidence", "")))
    else:
        L.append("（无）")
    L.append("")
    L.append("## 人工判级（范围外能力）")
    L.append("")
    if oos:
        L.append("| 能力 | 判级 | 证据 | 备注 |")
        L.append("|---|---|---|---|")
        for it in oos:
            L.append("| " + str(it.get("name")) + " | " + str(it.get("grade"))
                     + " | " + str(it.get("evidence")) + " | " + str(it.get("note", "")) + " |")
    else:
        L.append("（无；docs/design/capability-closure-overrides.json 的 out_of_scope 数组驱动）")
    L.append("")
    L.append("## UNWIRED")
    L.append("")
    unwired = sorted(by_status.get("UNWIRED", []), key=lambda x: x["name"])
    if unwired:
        for r in unwired:
            L.append("- " + r["name"] + " - 合约行 " + str(r["contract_line"])
                     + "；无处理器注册（flow 命令由 scheduler.lua 内联分发；详见局限）")
    else:
        L.append("（无）")
    L.append("")
    L.append("## PARTIAL")
    L.append("")
    partial = sorted(by_status.get("PARTIAL", []), key=lambda x: x["name"])
    if partial:
        for r in partial:
            L.append("- " + r["name"] + " - " + str(r["evidence"])
                     + "；处理器体未以调用形触达效果面（v2 已剥注释与字符串字面量）")
    else:
        L.append("（无）")
    L.append("")
    L.append("## EXTRA")
    L.append("")
    extra = sorted(by_status.get("EXTRA", []), key=lambda x: x["name"])
    if extra:
        for r in extra:
            L.append("- " + r["name"] + " - " + str(r["evidence"])
                     + "；" + str(r.get("source_type") or "?") + "（已注册但无合约条目）")
            if (r.get("source_type") or "").startswith("A:"):
                L.append("      - A 类：owner=contracts 生成链（kag/schema.lua -> schema_doc.lua dump）；入册与否=产品决策（待议）")
    else:
        L.append("（无）")
    L.append("")
    L.append("## 私有辅助（_ 前缀，注册但不属命令面）")
    L.append("")
    if private:
        for name, f, ln in sorted(private):
            L.append("- " + name + " - " + f + ":" + str(ln))
    else:
        L.append("（无）")
    L.append("")
    L.append("## 数据与判级局限（v2）")
    L.append("")
    L.append("1. **Consumed 为调用形文本启发式**：注释与字符串字面量先被剥离（strip_lua 状态机），")
    L.append("   然后要求 backend./layers./kag. 的 <ident>( 调用形，或 ctx.tf. / ctx.tf[ / ctx.tf= 字段/赋值。")
    L.append("   仍可能低估（经本地别名或工具函数间接调用时本体不含直接调用：palette 命令经 palette 模块间接生效，")
    L.append("   如实归 PARTIAL），也可能高估（kag. 自派发计入）。脚本不执行 Lua。")
    L.append("2. **Dispatched 为静态解析**：commands 导出表函数/赋值键 + kag.lua 显式映射与 function KAG.x 定义 + ")
    L.append("   sma_commands 子表。jump/call/endmacro 等直接 API 计入 EXTRA(api-alias)；[jump]/[if] 等 token 的")
    L.append("   流控处理由 scheduler.lua 编译期内联；两条轨道并存，本扫描器按注册键计 Dispatched。")
    L.append("3. **Tested 为原始引用计数**：tests/scripts/*.lua 与 web/*.test.js 中 [<name> 或 kag.<name> 出现次数，")
    L.append("   不区分断言与非断言上下文（注释/数组/字符串也算）。")
    L.append("4. **Observable / Platform Tested / Packaged 首版为 ?**，可由 overrides JSON 人工覆盖（⚠ 标记）；")
    L.append("   平台运行矩阵/打包验证由 M4/release 验证工件补证。")
    L.append("5. 判级只依赖命令名静态匹配；同名异构（如 vfx 的 flash 与 transition 的 flash）以注册表实际键为准。")
    L.append("   导出表引用的子表（如 TransCommands.Bezier = Bezier）经 pairs() 一并注册为调度键——EXTRA")
    L.append("   (subtable-key)，非用户命令面。")
    L.append("6. 合约计数以 command-contracts.md 的 ### 条目数为准（表头标注 134 须一致）。")
    L.append("7. overrides JSON 的 commands 键必须落在已知命令名集合内；未知键被响亮拒绝（exit 非 0），")
    L.append("   绝不静默忽略。")
    L.append("")
    L.append("## 复现")
    L.append("")
    L.append("```")
    L.append("python scripts/capability_closure.py")
    L.append("```")
    L.append("")
    return NL.join(L) + NL


def source_fingerprint():
    """sha256 aggregate over input CONTENT (sorted relpath + bytes), first 16 hex.
    The deterministic-output guarantee is against this fingerprint: same
    input content => same fingerprint => same bytes out. generated_at stays
    (newest input mtime) but cross-machine checkout mtimes differ by design,"""
    import hashlib
    h = hashlib.sha256()
    paths = [CONTRACTS, KAG_LUA, SMA_LUA, OVERRIDES]
    paths += sorted(COMMANDS_DIR.glob("*.lua"))
    if TESTS_LUA.is_dir():
        paths += sorted(TESTS_LUA.rglob("*.lua"))
    if TESTS_WEB.is_dir():
        paths += sorted(TESTS_WEB.glob("*.test.js"))
    for p in paths:
        if not p.exists():
            continue
        h.update(str(p.relative_to(REPO)).encode("utf-8"))
        h.update(chr(0).encode("utf-8"))
        h.update(p.read_bytes())
    return h.hexdigest()[:16]


def main():
    global SUBTABLE_NAMES
    SUBTABLE_NAMES = collect_subtable_names()
    declared_names_probe = set(parse_contracts())
    handlers_probe, _ = parse_command_modules()
    handlers_probe.update(parse_sma_commands())
    _, kag_defs_probe = parse_kag_lua_mappings()
    handlers_probe.update(kag_defs_probe)
    known = sorted(set(declared_names_probe) | set(handlers_probe))
    overrides_map, oos = load_overrides(set(known))
    records, private, declared_total = build_records(overrides_map)
    guard_partial(records)
    guard_partial_extra()
    fp = source_fingerprint()
    generated_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(source_mtime([
        CONTRACTS, KAG_LUA, SMA_LUA, COMMANDS_DIR, TESTS_LUA, TESTS_WEB, OVERRIDES])))
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "generated_at": generated_at,
        "source_fingerprint": fp,
        "scanner": "scripts/capability_closure.py",
        "sources": {
            "contracts": str(CONTRACTS.relative_to(REPO)),
            "command_modules": str(COMMANDS_DIR.relative_to(REPO)),
            "kag_lua": str(KAG_LUA.relative_to(REPO)),
            "sma_lua": str(SMA_LUA.relative_to(REPO)),
            "tests_scripts": str(TESTS_LUA.relative_to(REPO)),
            "tests_web": str(TESTS_WEB.relative_to(REPO)),
            "overrides": str(OVERRIDES.relative_to(REPO)),
        },
        "counts": {
            "declared": declared_total,
            "dispatched": sum(1 for r in records if r["dispatched"]),
            "consumed": sum(1 for r in records if r["dispatched"] and r["consumed"]),
            "tested": sum(1 for r in records if r["tested_count"] > 0),
        },
        "status_counts": {st: sum(1 for r in records if r["status"] == st)
                          for st in ("UNWIRED", "PARTIAL", "CLOSED", "EXTRA")},
        "commands": records,
        "private_helpers": [{"name": n, "file": f, "line": ln} for n, f, ln in private],
    }
    with open(OUT_JSON, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, ensure_ascii=False, indent=2)
        fh.write(NL)
    md = render_markdown(records, private, declared_total, oos, generated_at, fp)
    OUT_MD.parent.mkdir(parents=True, exist_ok=True)
    OUT_MD.write_text(md, encoding="utf-8")
    print("[closure] declared=" + str(declared_total)
          + " dispatched=" + str(payload["counts"]["dispatched"])
          + " consumed=" + str(payload["counts"]["consumed"])
          + " tested=" + str(payload["counts"]["tested"]))
    print("[closure] statuses=" + str(payload["status_counts"]))
    print("[closure] overrides=" + str(len(overrides_map)) + " out_of_scope=" + str(len(oos)))
    print("[closure] json -> " + str(OUT_JSON.relative_to(REPO))
          + "  md -> " + str(OUT_MD.relative_to(REPO)))
    return 0


def guard_partial_extra():
    # (intentionally empty hook for future t107 delta review locks)
    pass


if __name__ == "__main__":
    sys.exit(main())