#!/usr/bin/env python3
"""api_stats.py — authoritative API census for the Caesura engine.

Scans the source tree (interfaces, Lua bindings, KAG commands, RPC
endpoints) and emits a Markdown statistics document. Every number is
derived from the code (or from live test runs), never hand-maintained.

Usage: python scripts/api_stats.py [--json]
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def scan_interfaces():
    """src/<module>/api/I*.h — pure-virtual method census."""
    modules = {}
    total_methods = 0
    for mod in sorted(os.listdir(os.path.join(ROOT, "src"))):
        api_dir = os.path.join(ROOT, "src", mod, "api")
        if not os.path.isdir(api_dir):
            continue
        ifaces = []
        for fn in sorted(os.listdir(api_dir)):
            if not (fn.startswith("I") and fn.endswith(".h")):
                continue
            path = os.path.join(api_dir, fn)
            text = open(path, encoding="utf-8", errors="replace").read()
            methods = len(re.findall(r"\)\s*=\s*0\s*;", text))
            ifaces.append({"file": fn, "methods": methods})
        if ifaces:
            modules[mod] = ifaces
            total_methods += sum(i["methods"] for i in ifaces)
    return modules, total_methods


def scan_lua_bindings():
    """src/script/bindings/*.cpp luaL_Reg tables + registration prints."""
    bindings = {}
    bind_dir = os.path.join(ROOT, "src", "script", "bindings")
    for fn in sorted(os.listdir(bind_dir)):
        if not fn.endswith(".cpp"):
            continue
        text = open(os.path.join(bind_dir, fn), encoding="utf-8", errors="replace").read()
        # luaL_Reg array entries: { "name", func },
        regs = re.findall(r'\{\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,\s*[A-Za-z_][A-Za-z0-9_]*\s*\}', text)
        # registration target: lua_setglobal(L, "name") or setfuncs on KAG
        globals_ = re.findall(r'lua_setglobal\([^,]+,\s*"([A-Za-z_][A-Za-z0-9_]*)"\)', text)
        if regs:
            bindings[fn] = {"apis": regs, "globals": globals_}
    return bindings


def scan_kag_commands():
    """scripts/kag/commands/ + command-contracts.md census."""
    cmd_dir = os.path.join(ROOT, "scripts", "kag", "commands")
    files = sorted(os.listdir(cmd_dir)) if os.path.isdir(cmd_dir) else []
    contracts = os.path.join(ROOT, "docs", "api", "command-contracts.md")
    contract_count = 0
    if os.path.isfile(contracts):
        text = open(contracts, encoding="utf-8").read()
        contract_count = len(re.findall(r"^#{2,3} .*`\[", text, re.M)) or \
            len(re.findall(r"`\[[a-z_]+", text))
    return {"files": files, "contract_commands": contract_count}


def scan_rpc():
    """EditorServer endpoints (svr.Get/Post) + stdin dispatcher methods."""
    path = os.path.join(ROOT, "src", "rpc", "EditorServer.cpp")
    endpoints = []
    if os.path.isfile(path):
        text = open(path, encoding="utf-8", errors="replace").read()
        endpoints = re.findall(r'svr\.(Get|Post)\("([^"]+)"', text)
    # stdin JSON-RPC: RpcServer.cpp uses a method == "..." if-chain.
    stdin_methods = []
    rpc_path = os.path.join(ROOT, "src", "rpc", "RpcServer.cpp")
    if os.path.isfile(rpc_path):
        text = open(rpc_path, encoding="utf-8", errors="replace").read()
        stdin_methods = sorted(set(re.findall(r'method\s*==\s*"([a-zA-Z_][a-zA-Z0-9_]*)"', text)))
    return {"http_endpoints": endpoints, "stdin_methods": stdin_methods}


def count_lua_scripts():
    scripts_dir = os.path.join(ROOT, "scripts")
    count = 0
    for root, _, files in os.walk(scripts_dir):
        if "check" in root or "demo" in root:
            continue
        count += sum(1 for f in files if f.endswith(".lua"))
    return count


def run_tests():
    """Live test census (best effort; returns None when not runnable)."""
    result = {"cpp_cases": None, "cpp_asserts": None, "lua_passed": None}
    try:
        exe = os.path.join(ROOT, "build", "tests", "Debug", "CaesuraTests.exe")
        if os.path.isfile(exe):
            out = subprocess.run([exe, "-r", "compact"], capture_output=True, text=True, encoding="utf-8", errors="replace",
                                 timeout=300, cwd=os.path.dirname(exe)).stdout
            matches = re.findall(r"test cases:\s*(\d+)\s*\|\s*(\d+)\s+passed.*?assertions:\s*(\d+)\s*\|\s*(\d+)\s+passed",
                                 out, re.DOTALL)
            if matches:
                # Last block is the grand total; earlier blocks are
                # per-suite summaries (compact reporter).
                result["cpp_cases"], result["cpp_asserts"] = int(matches[-1][1]), int(matches[-1][3])
    except Exception:
        pass
    try:
        lua = os.path.join(ROOT, "external", "lua", "lua.exe")
        if os.path.isfile(lua):
            out = subprocess.run([lua, os.path.join(ROOT, "tests", "scripts", "run_lua_tests.lua")],
                                 capture_output=True, text=True, timeout=300,
                                 cwd=ROOT).stdout
            matches = re.findall(r"Results:\s*(\d+)\s+passed,\s*\d+\s+failed,\s*\d+\s+total", out)
            if matches:
                result["lua_passed"] = int(matches[-1])
    except Exception:
        pass
    return result


def main():
    interfaces, total_iface_methods = scan_interfaces()
    bindings = scan_lua_bindings()
    kag = scan_kag_commands()
    rpc = scan_rpc()
    lua_scripts = count_lua_scripts()
    tests = run_tests()

    stats = {
        "modules": sorted(interfaces.keys()),
        "interface_files": sum(len(v) for v in interfaces.values()),
        "interface_methods": total_iface_methods,
        "bindings": bindings,
        "binding_apis": sum(len(v["apis"]) for v in bindings.values()),
        "kag_command_files": len(kag["files"]),
        "kag_contract_commands": kag["contract_commands"],
        "rpc_http_endpoints": len(rpc["http_endpoints"]),
        "rpc_stdin_methods": len(rpc["stdin_methods"]),
        "lua_scripts": lua_scripts,
        "tests": tests,
    }

    if "--json" in sys.argv:
        print(json.dumps(stats, indent=2, ensure_ascii=False))
        return

    lines = []
    A = lines.append
    A("# Caesura (AmeKAG) — API Statistics")
    A("")
    A("> Auto-generated by `scripts/api_stats.py`. Every number is derived from the")
    A("> source tree or a live test run — never hand-maintained. Regenerate with:")
    A("> `python scripts/api_stats.py`")
    A("")
    A("## 1. Surface summary")
    A("")
    A("| Metric | Count |")
    A("|--------|-------|")
    A(f"| Module libraries (src/) | {len(stats['modules'])} |")
    A(f"| API interface headers (src/*/api/I*.h) | {stats['interface_files']} |")
    A(f"| Pure-virtual interface methods | {stats['interface_methods']} |")
    A(f"| Lua binding functions (luaL_Reg entries) | {stats['binding_apis']} |")
    A(f"| KAG command handler files | {stats['kag_command_files']} |")
    A(f"| KAG contract commands (command-contracts.md) | {stats['kag_contract_commands']} |")
    A(f"| RPC HTTP endpoints (EditorServer) | {stats['rpc_http_endpoints']} |")
    A(f"| RPC stdin JSON-RPC methods | {stats['rpc_stdin_methods']} |")
    A(f"| Lua runtime scripts (scripts/, excl. demo/check) | {stats['lua_scripts']} |")
    if tests["cpp_cases"] is not None:
        A(f"| C++ test cases | {tests['cpp_cases']} |")
    if tests["cpp_asserts"] is not None:
        A(f"| C++ assertions | {tests['cpp_asserts']} |")
    if tests["lua_passed"] is not None:
        A(f"| Lua tests passed | {tests['lua_passed']} |")
    A("")
    A("## 2. C++ interfaces by module")
    A("")
    A("| Module | Interface file | Pure-virtual methods |")
    A("|--------|----------------|----------------------|")
    for mod in stats["modules"]:
        for iface in interfaces[mod]:
            A(f"| {mod} | {iface['file']} | {iface['methods']} |")
    A("")
    A("## 3. Lua bindings by source file")
    A("")
    A("| Binding file | API count | Registered globals |")
    A("|--------------|-----------|--------------------|")
    for fn, info in bindings.items():
        A(f"| {fn} | {len(info['apis'])} | {', '.join(info['globals']) or '—'} |")
    A("")
    A("## 4. RPC surface")
    A("")
    A("### 4.1 HTTP endpoints (EditorServer)")
    A("")
    A("| Method | Path |")
    A("|--------|------|")
    for m, p in sorted(rpc["http_endpoints"]):
        A(f"| {m} | {p} |")
    A("")
    A("### 4.2 stdin JSON-RPC methods")
    A("")
    for m in rpc["stdin_methods"]:
        A(f"- `{m}`")
    A("")
    A("## 5. KAG command handlers")
    A("")
    A("| File |")
    A("|------|")
    for f in kag["files"]:
        A(f"| {f} |")

    out = os.path.join(ROOT, "docs", "api", "api-stats.md")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Wrote {out} ({len(lines)} lines)")


if __name__ == "__main__":
    main()
