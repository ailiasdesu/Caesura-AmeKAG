#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Automated i18n Extraction Tool
Extracts translatable text from KAG .ks scripts and outputs CSV / gettext PO templates.
"""

import sys, os, re, argparse, csv, hashlib

def extract_strings_from_ks(ks_path):
    with open(ks_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
        
    entries = []
    scene_name = os.path.basename(ks_path)
    
    for idx, raw_line in enumerate(lines, 1):
        line = raw_line.strip()
        if not line or line.startswith(';'):
            continue
            
        # Character dialogue / Narration: [ch text="..."] or [ch name="..." text="..."]
        ch_matches = re.finditer(r'\[ch\s+([^\]]+)\]', line)
        for m in ch_matches:
            args = m.group(1)
            speaker_m = re.search(r'name=[\"\']?([^\"\'\s\]]+)', args)
            text_m = re.search(r'text=[\"\']([^\"\']+)[\"\']', args)
            if text_m:
                txt = text_m.group(1)
                spk = speaker_m.group(1) if speaker_m else 'Narrator'
                key = f"{scene_name}:L{idx}:{hashlib.md5(txt.encode('utf-8')).hexdigest()[:6]}"
                entries.append({
                    'key': key,
                    'file': scene_name,
                    'line': idx,
                    'speaker': spk,
                    'source': txt
                })
                
        # Choice buttons: [sel text="..."]
        sel_matches = re.finditer(r'\[sel\s+([^\]]+)\]', line)
        for m in sel_matches:
            args = m.group(1)
            text_m = re.search(r'text=[\"\']([^\"\']+)[\"\']', args)
            if text_m:
                txt = text_m.group(1)
                key = f"{scene_name}:L{idx}:choice:{hashlib.md5(txt.encode('utf-8')).hexdigest()[:6]}"
                entries.append({
                    'key': key,
                    'file': scene_name,
                    'line': idx,
                    'speaker': 'System_Choice',
                    'source': txt
                })
                
        # Toast notifications: [notify msg="..."]
        notify_matches = re.finditer(r'\[notify\s+([^\]]+)\]', line)
        for m in notify_matches:
            args = m.group(1)
            msg_m = re.search(r'msg=[\"\']([^\"\']+)[\"\']', args)
            if msg_m:
                txt = msg_m.group(1)
                key = f"{scene_name}:L{idx}:notify:{hashlib.md5(txt.encode('utf-8')).hexdigest()[:6]}"
                entries.append({
                    'key': key,
                    'file': scene_name,
                    'line': idx,
                    'speaker': 'System_Notify',
                    'source': txt
                })
                
    return entries

def export_csv(entries, out_path):
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8-sig', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Key', 'File', 'Line', 'Speaker', 'SourceText', 'Translation_zh', 'Translation_en', 'Translation_ja'])
        for e in entries:
            writer.writerow([e['key'], e['file'], e['line'], e['speaker'], e['source'], '', '', ''])
    print(f"Exported {len(entries)} translatable strings to {out_path}")

def export_po(entries, out_path, lang='zh'):
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(f'''msgid ""
msgstr ""
"Project-Id-Version: Caesura Visual Novel\\n"
"Language: {lang}\\n"
"MIME-Version: 1.0\\n"
"Content-Type: text/plain; charset=UTF-8\\n"
"Content-Transfer-Encoding: 8bit\\n"

''')
        for e in entries:
            f.write(f"#: {e['file']}:{e['line']} (Speaker: {e['speaker']})\n")
            f.write(f"msgctxt \"{e['key']}\"\n")
            f.write(f"msgid \"{e['source']}\"\n")
            f.write(f"msgstr \"\"\n\n")
    print(f"Exported {len(entries)} translatable strings to PO template: {out_path}")

def main():
    parser = argparse.ArgumentParser(description='Caesura i18n Extraction Tool')
    parser.add_argument('input', help='Path to .ks file or project directory')
    parser.add_argument('--out', '-o', required=True, help='Output CSV or PO file path')
    parser.add_argument('--format', choices=['csv', 'po'], default='csv')
    parser.add_argument('--lang', default='zh', help='Target language for PO template')
    args = parser.parse_args()
    
    ks_files = []
    if os.path.isfile(args.input):
        ks_files = [args.input]
    elif os.path.isdir(args.input):
        for root, _, files in os.walk(args.input):
            for f in sorted(files):
                if f.endswith('.ks'):
                    ks_files.append(os.path.join(root, f))
    else:
        print(f"Error: input path not found: {args.input}", file=sys.stderr)
        sys.exit(1)
        
    all_entries = []
    for ksf in ks_files:
        all_entries.extend(extract_strings_from_ks(ksf))
        
    if args.format == 'csv':
        export_csv(all_entries, args.out)
    elif args.format == 'po':
        export_po(all_entries, args.out, args.lang)

if __name__ == '__main__':
    main()
