#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Story Flow Graph Generator (Unified Semantic Layer)
Statically parses KAG .ks scripts and constructs visual story branching topologies.
Outputs Mermaid flowchart, JSON, and diagnostics for broken/unreachable jump targets.
"""

import sys, os, subprocess, argparse

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

def run_story_flow(input_path, out_path=None, fmt='mermaid', lint=False):
    lua_bin = find_lua()
    script_path = os.path.join('scripts', 'kag_semantic.lua')
    
    cmd = [
        lua_bin,
        script_path,
        'flow',
        input_path,
        '--format', fmt,
    ]
    if lint:
        cmd.append('--lint')
    if out_path:
        cmd.extend(['-o', out_path])
        
    res = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='ignore')
    if res.returncode != 0:
        print(f"Error generating story flow:\n{res.stderr or res.stdout}", file=sys.stderr)
        sys.exit(res.returncode)
    else:
        output = res.stdout.strip()
        if output:
            print(output)

def main():
    parser = argparse.ArgumentParser(description='Caesura Story Flow Graph Generator (Semantic Layer)')
    parser.add_argument('input', help='Path to .ks file or directory')
    parser.add_argument('--out', '-o', help='Output file path')
    parser.add_argument('--format', choices=['mermaid', 'json', 'markdown'], default='mermaid')
    parser.add_argument('--lint', action='store_true', help='Run flow diagnostics')
    args = parser.parse_args()
    
    run_story_flow(args.input, args.out, args.format, args.lint)

if __name__ == '__main__':
    main()
