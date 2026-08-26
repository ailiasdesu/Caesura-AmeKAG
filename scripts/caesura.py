#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Creator Unified Command Line Interface (CLI)
Unified interface for project scaffolding, doctor diagnostics, story flow, and i18n.
"""

import sys, os, subprocess, shutil, argparse

def find_lua():
    candidates = [
        os.path.join('external', 'lua', 'lua.exe'),
        os.path.join('external', 'lua', 'lua'),
        'lua',
        'lua5.4',
    ]
    for c in candidates:
        if os.path.exists(c) and (os.access(c, os.X_OK) or os.name == 'nt'):
            return c
    return 'lua'

def cmd_doctor(args):
    print("=== Caesura Environment Doctor ===\n")
    tools = [
        ("Python 3", [sys.executable, "--version"], "Required for creator toolchain"),
        ("Lua 5.4", [find_lua(), "-v"], "Core script runtime"),
        ("Node.js / npm", ["npm", "--version"], "Web player test & packaging"),
        ("CMake", ["cmake", "--version"], "C++ build system"),
        ("Git", ["git", "--version"], "Version control"),
    ]
    
    passed = 0
    for name, cmd, desc in tools:
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            if res.returncode == 0:
                first_line = (res.stdout or res.stderr).strip().splitlines()[0]
                print(f"  [OK] {name:<16} : {first_line}")
                passed += 1
            else:
                print(f"  [WARN] {name:<14} : Exited with code {res.returncode}")
        except Exception as e:
            print(f"  [FAIL] {name:<14} : Not found ({desc})")
            
    print(f"\nDoctor check complete: {passed}/{len(tools)} critical tools ready.")
    return 0

def cmd_create(args):
    name = args.name
    template = args.template or "showcase"
    target_dir = args.out or name
    
    template_src = os.path.join("tools", "project_templates", template)
    if not os.path.exists(template_src):
        # Fallback to demo/example_game if showcase requested
        if template == "showcase" and os.path.exists("demo/example_game"):
            template_src = "demo/example_game"
        else:
            print(f"Error: template '{template}' not found in tools/project_templates/", file=sys.stderr)
            return 1
            
    if os.path.exists(target_dir):
        print(f"Error: target directory '{target_dir}' already exists.", file=sys.stderr)
        return 1
        
    shutil.copytree(template_src, target_dir)
    print(f"[OK] Project '{name}' created from template '{template}' at: {target_dir}")
    return 0

def cmd_flow(args):
    lua = find_lua()
    cmd = [lua, "scripts/kag_semantic.lua", "flow", args.path, "--format", args.format]
    if args.lint:
        cmd.append("--lint")
    if args.out:
        cmd.extend(["-o", args.out])
    res = subprocess.run(cmd)
    return res.returncode

def cmd_i18n(args):
    target = args.path
    if args.extract:
        out_csv = args.out or os.path.join(target, "i18n", "locales.csv")
        os.makedirs(os.path.dirname(out_csv), exist_ok=True)
        res = subprocess.run([sys.executable, "scripts/extract_i18n.py", target, "--out", out_csv])
        return res.returncode
    elif args.lint:
        dict_dir = args.dir or (os.path.join(target, "i18n") if os.path.isdir(target) else "i18n")
        res = subprocess.run([sys.executable, "scripts/lint_i18n.py", "--dir", dict_dir])
        return res.returncode
    else:
        # Full extraction
        out_csv = args.out or os.path.join(target, "locales.csv")
        res = subprocess.run([sys.executable, "scripts/extract_i18n.py", target, "--out", out_csv])
        return res.returncode

def cmd_check(args):
    lua = find_lua()
    res = subprocess.run([lua, "scripts/ks_check.lua", args.path])
    return res.returncode

def cmd_patch(args):
    raw_args = args.patch_args
    if not raw_args:
        print("Usage: caesura patch <base.carc> <target.carc> <delta.carc>")
        print("       caesura patch create <base.carc> <target.carc> <delta.carc>")
        print("       caesura patch apply <base.carc> <delta.carc> <output.carc>")
        print("       caesura patch verify <delta.carc>")
        return 1

    first = raw_args[0]
    if first == "create" and len(raw_args) >= 4:
        cmd = [sys.executable, "scripts/carc_pack.py", "delta", raw_args[1], raw_args[2], raw_args[3]]
    elif first == "apply" and len(raw_args) >= 4:
        cmd = [sys.executable, "scripts/carc_pack.py", "apply", raw_args[1], raw_args[2], raw_args[3]]
    elif first == "verify" and len(raw_args) >= 2:
        cmd = [sys.executable, "scripts/carc_pack.py", "verify-delta", raw_args[1]]
    elif len(raw_args) == 3:
        # Default: <base.carc> <target.carc> <delta.carc>
        cmd = [sys.executable, "scripts/carc_pack.py", "delta", raw_args[0], raw_args[1], raw_args[2]]
    else:
        print(f"Error: invalid patch arguments: {' '.join(raw_args)}", file=sys.stderr)
        return 1

    res = subprocess.run(cmd)
    return res.returncode

def main():
    parser = argparse.ArgumentParser(prog="caesura", description="Caesura (AmeKAG) Creator CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    # doctor
    p_doc = subparsers.add_parser("doctor", help="Inspect local development environment")
    p_doc.set_defaults(func=cmd_doctor)
    
    # create
    p_create = subparsers.add_parser("create", help="Create a new visual novel project")
    p_create.add_argument("name", help="Project name")
    p_create.add_argument("--template", choices=["showcase", "basic", "blank", "live2d", "kag3"], default="showcase")
    p_create.add_argument("-o", "--out", help="Target output directory")
    p_create.set_defaults(func=cmd_create)
    
    # flow
    p_flow = subparsers.add_parser("flow", help="Generate story flow graph & diagnostics")
    p_flow.add_argument("path", help="Path to .ks scene or project directory")
    p_flow.add_argument("--format", choices=["mermaid", "json", "markdown"], default="mermaid")
    p_flow.add_argument("--lint", action="store_true", help="Run broken jump / orphan diagnostics")
    p_flow.add_argument("-o", "--out", help="Output file path")
    p_flow.set_defaults(func=cmd_flow)
    
    # i18n
    p_i18n = subparsers.add_parser("i18n", help="Manage localization pipeline")
    p_i18n.add_argument("path", help="Path to .ks scene or project directory")
    p_i18n.add_argument("--extract", action="store_true", help="Extract translatables to CSV")
    p_i18n.add_argument("--lint", action="store_true", help="Audit translation coverage")
    p_i18n.add_argument("--dir", help="Directory containing <lang>.json dictionaries for linting")
    p_i18n.add_argument("-o", "--out", help="Output CSV path")
    p_i18n.set_defaults(func=cmd_i18n)
    
    # check
    p_check = subparsers.add_parser("check", help="Statically validate .ks contracts")
    p_check.add_argument("path", help="Path to .ks scene")
    p_check.set_defaults(func=cmd_check)
    
    # patch
    p_patch = subparsers.add_parser("patch", help="Differential CARC patch creation, application, and verification")
    p_patch.add_argument("patch_args", nargs="*", help="Arguments: <base.carc> <target.carc> <delta.carc> or create|apply|verify ...")
    p_patch.set_defaults(func=cmd_patch)
    
    args = parser.parse_args()
    sys.exit(args.func(args))

if __name__ == "__main__":
    main()
