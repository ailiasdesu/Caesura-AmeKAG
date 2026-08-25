#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Automated i18n Extraction Tool (Unified Semantic Layer)
Extracts translatable text from KAG .ks scripts via kag_semantic.lua.
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
        if os.path.exists(c) and os.access(c, os.X_OK):
            return c
        # On Windows, check with PATH
        if os.name == 'nt' and os.path.exists(c):
            return c
    return 'lua'

def extract_i18n(input_path, out_path, fmt='csv', lang='zh'):
    lua_bin = find_lua()
    script_path = os.path.join('scripts', 'kag_semantic.lua')
    
    cmd = [
        lua_bin,
        script_path,
        'i18n',
        input_path,
        '--format', fmt,
        '--lang', lang,
        '-o', out_path
    ]
    
    res = subprocess.run(cmd, capture_output=True, encoding='utf-8', errors='ignore')
    if res.returncode != 0:
        print(f"Error extracting i18n via semantic layer:\n{res.stderr or res.stdout}", file=sys.stderr)
        sys.exit(res.returncode)
    else:
        print(res.stdout.strip())

def main():
    parser = argparse.ArgumentParser(description='Caesura i18n Extraction Tool (Semantic Layer)')
    parser.add_argument('input', help='Path to .ks file or project directory')
    parser.add_argument('--out', '-o', required=True, help='Output CSV or PO file path')
    parser.add_argument('--format', choices=['csv', 'po'], default='csv')
    parser.add_argument('--lang', default='zh', help='Target language for PO template')
    args = parser.parse_args()
    
    extract_i18n(args.input, args.out, args.format, args.lang)

if __name__ == '__main__':
    main()
