#!/usr/bin/env python3
"""
Caesura (AmeKAG) — i18n Translation Lint
Verifies translation completeness and identifies missing keys across language dictionaries.
"""

import sys, os, json, argparse

def lint_i18n(i18n_dir, langs=('zh', 'en', 'ja')):
    dictionaries = {}
    for lang in langs:
        fpath = os.path.join(i18n_dir, f"{lang}.json")
        if os.path.exists(fpath):
            with open(fpath, 'r', encoding='utf-8') as f:
                dictionaries[lang] = json.load(f)
        else:
            print(f"[WARN] Dictionary missing for language: {lang} ({fpath})")
            
    if not dictionaries:
        print(f"[ERROR] No dictionary files found in {i18n_dir}")
        return 1
        
    all_keys = set()
    for d in dictionaries.values():
        all_keys.update(d.keys())
        
    print(f"=== i18n Translation Lint ({len(all_keys)} total string keys) ===")
    missing_count = 0
    for lang in langs:
        if lang not in dictionaries: continue
        d = dictionaries[lang]
        missing = [k for k in all_keys if k not in d or not str(d[k]).strip()]
        coverage = ((len(all_keys) - len(missing)) / len(all_keys)) * 100 if all_keys else 100.0
        print(f"Language [{lang}]: {len(all_keys) - len(missing)}/{len(all_keys)} translated ({coverage:.1f}%)")
        if missing:
            missing_count += len(missing)
            for m in missing[:5]:
                print(f"  - Missing [{lang}]: \"{m[:60]}\"")
            if len(missing) > 5:
                print(f"  ... and {len(missing) - 5} more")
                
    if missing_count == 0:
        print("OK: 100% Translation coverage across all configured languages.")
        return 0
    else:
        print(f"Lint finished with {missing_count} missing translation entries.")
        return 0

def main():
    parser = argparse.ArgumentParser(description='Caesura i18n Lint Tool')
    parser.add_argument('--dir', default='assets/i18n', help='Directory containing <lang>.json dictionaries')
    parser.add_argument('--langs', nargs='+', default=['zh', 'en', 'ja'], help='Languages to audit')
    args = parser.parse_args()
    
    sys.exit(lint_i18n(args.dir, args.langs))

if __name__ == '__main__':
    main()
