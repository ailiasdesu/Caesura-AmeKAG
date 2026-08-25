#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Automated i18n Import Tool
Compiles translated CSV / PO files into engine-ready JSON localization dictionaries.
"""

import sys, os, csv, json, argparse, re

def import_csv(csv_path, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    translations = {
        'zh': {},
        'en': {},
        'ja': {}
    }
    
    with open(csv_path, 'r', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        for row in reader:
            src = row.get('SourceText', '').strip()
            if not src: continue
            
            zh = row.get('Translation_zh', '').strip() or src
            en = row.get('Translation_en', '').strip() or src
            ja = row.get('Translation_ja', '').strip() or src
            
            translations['zh'][src] = zh
            translations['en'][src] = en
            translations['ja'][src] = ja
            
    for lang, dic in translations.items():
        out_file = os.path.join(out_dir, f"{lang}.json")
        with open(out_file, 'w', encoding='utf-8') as f:
            json.dump(dic, f, indent=2, ensure_ascii=False)
        print(f"Generated {out_file} ({len(dic)} entries)")

def import_po(po_path, out_file):
    os.makedirs(os.path.dirname(os.path.abspath(out_file)), exist_ok=True)
    with open(po_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
    dic = {}
    blocks = content.split('\n\n')
    for b in blocks:
        msgid_m = re.search(r'msgid\s+\"([^\"]+)\"', b)
        msgstr_m = re.search(r'msgstr\s+\"([^\"]*)\"', b)
        if msgid_m and msgstr_m:
            src = msgid_m.group(1)
            trans = msgstr_m.group(1) or src
            dic[src] = trans
            
    with open(out_file, 'w', encoding='utf-8') as f:
        json.dump(dic, f, indent=2, ensure_ascii=False)
    print(f"Generated {out_file} ({len(dic)} entries)")

def main():
    parser = argparse.ArgumentParser(description='Caesura i18n Import Tool')
    parser.add_argument('input', help='Path to translated CSV or PO file')
    parser.add_argument('--out-dir', default='assets/i18n', help='Output directory for JSON dictionaries')
    parser.add_argument('--po-out', help='Output JSON file path when importing single PO file')
    args = parser.parse_args()
    
    if args.input.endswith('.csv'):
        import_csv(args.input, args.out_dir)
    elif args.input.endswith('.po'):
        out_file = args.po_out or os.path.join(args.out_dir, 'imported.json')
        import_po(args.input, out_file)
    else:
        print("Error: input must be .csv or .po file", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
