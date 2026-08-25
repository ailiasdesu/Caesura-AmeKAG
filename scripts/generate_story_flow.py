#!/usr/bin/env python3
"""
Caesura (AmeKAG) — Story Flow Graph Generator
Statically parses KAG .ks scripts and constructs visual story branching topologies.
Outputs Mermaid flowchart, JSON, and diagnostics for broken/unreachable jump targets.
"""

import sys, os, re, argparse, json

def parse_ks_flow(ks_path):
    with open(ks_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    nodes = {}
    current_label = '*_entry_'
    nodes[current_label] = {'title': 'Entry', 'choices': [], 'jumps': [], 'calls': [], 'has_ending': False, 'line': 1}
    for idx, raw_line in enumerate(lines, 1):
        line = raw_line.strip()
        if not line or line.startswith(';'):
            continue
        if line.startswith('*'):
            parts = line[1:].split('|')
            lbl_name = '*' + parts[0].strip()
            title = parts[1].strip() if len(parts) > 1 else parts[0].strip()
            current_label = lbl_name
            if current_label not in nodes:
                nodes[current_label] = {'title': title, 'choices': [], 'jumps': [], 'calls': [], 'has_ending': False, 'line': idx}
            continue
        sel_matches = re.findall(r'\[sel\s+([^\]]+)\]', line)
        for args in sel_matches:
            target_match = re.search(r'target=([*]?[\w_]+)', args)
            text_match = re.search(r'text=[\"\']?([^\"\'\s\]]+)', args)
            if target_match:
                tgt = target_match.group(1)
                if not tgt.startswith('*'): tgt = '*' + tgt
                txt = text_match.group(1) if text_match else 'Choice'
                nodes[current_label]['choices'].append({'target': tgt, 'text': txt, 'line': idx})
        jump_matches = re.findall(r'\[jump\s+([^\]]+)\]', line)
        for args in jump_matches:
            target_match = re.search(r'target=([*]?[\w_]+)', args)
            if target_match:
                tgt = target_match.group(1)
                if not tgt.startswith('*'): tgt = '*' + tgt
                nodes[current_label]['jumps'].append({'target': tgt, 'line': idx})
        call_matches = re.findall(r'\[call\s+([^\]]+)\]', line)
        for args in call_matches:
            target_match = re.search(r'target=([*]?[\w_]+)', args)
            if target_match:
                tgt = target_match.group(1)
                if not tgt.startswith('*'): tgt = '*' + tgt
                nodes[current_label]['calls'].append({'target': tgt, 'line': idx})
        if '[ending' in line:
            nodes[current_label]['has_ending'] = True
    return nodes

def generate_mermaid(nodes, scene_name='Story'):
    clean_id = lambda name: re.sub(r'[^a-zA-Z0-9_]', '_', name.lstrip('*'))
    lines = ['```mermaid', 'flowchart TD', f'    subgraph {clean_id(scene_name)} ["{scene_name}"]']
    for lbl, data in nodes.items():
        nid = clean_id(lbl)
        label_disp = data['title'].replace('"', '')
        if lbl == '*_entry_':
            lines.append(f'        {nid}(["Start: {scene_name}"])')
        elif data['has_ending']:
            lines.append(f'        {nid}(["Ending: {label_disp}"])')
        elif data['choices']:
            lines.append(f'        {nid}{{"{label_disp}"}}')
        else:
            lines.append(f'        {nid}["{label_disp}"]')
    lines.append('    end\n')
    for src_lbl, data in nodes.items():
        src_id = clean_id(src_lbl)
        for ch in data['choices']:
            tgt_id = clean_id(ch['target'])
            txt = ch['text'].replace('"', '')
            lines.append(f'    {src_id} -->|"{txt}"| {tgt_id}')
        for jmp in data['jumps']:
            tgt_id = clean_id(jmp['target'])
            lines.append(f'    {src_id} -.->|"jump"| {tgt_id}')
        for call in data['calls']:
            tgt_id = clean_id(call['target'])
            lines.append(f'    {src_id} ==>|"call"| {tgt_id}')
    lines.append('```')
    return '\n'.join(lines)

def lint_flow(nodes, scene_name='Story'):
    defined_labels = set(nodes.keys())
    referenced_labels = set()
    diagnostics = []
    for src_lbl, data in nodes.items():
        for ch in data['choices']:
            referenced_labels.add(ch['target'])
            if ch['target'] not in defined_labels:
                diagnostics.append(f'[{scene_name}:L{ch["line"]}] ERROR: Choice references undefined target {ch["target"]}')
        for jmp in data['jumps']:
            referenced_labels.add(jmp['target'])
            if jmp['target'] not in defined_labels:
                diagnostics.append(f'[{scene_name}:L{jmp["line"]}] ERROR: Jump references undefined target {jmp["target"]}')
        for call in data['calls']:
            referenced_labels.add(call['target'])
            if call['target'] not in defined_labels:
                diagnostics.append(f'[{scene_name}:L{call["line"]}] ERROR: Call references undefined target {call["target"]}')
    for lbl in defined_labels:
        if lbl != '*_entry_' and lbl not in referenced_labels:
            diagnostics.append(f'[{scene_name}:L{nodes[lbl]["line"]}] WARN: Label {lbl} is defined but never referenced')
    return diagnostics

def main():
    parser = argparse.ArgumentParser(description='Caesura Story Flow Graph Generator')
    parser.add_argument('input', help='Path to .ks file or directory')
    parser.add_argument('--out', '-o', help='Output file path')
    parser.add_argument('--format', choices=['mermaid', 'json', 'markdown'], default='mermaid')
    parser.add_argument('--lint', action='store_true', help='Run flow diagnostics')
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
        print(f'Error: input path not found: {args.input}', file=sys.stderr)
        sys.exit(1)
    all_mermaid = []
    all_json = {}
    all_diags = []
    for ksf in ks_files:
        sname = os.path.basename(ksf)
        nodes = parse_ks_flow(ksf)
        all_mermaid.append(generate_mermaid(nodes, sname))
        all_json[sname] = nodes
        if args.lint:
            all_diags.extend(lint_flow(nodes, sname))
    if args.lint:
        print(f'=== Story Flow Diagnostics ({len(ks_files)} scenes) ===')
        if not all_diags:
            print('OK: All story jumps, calls, and branches are sound.')
        else:
            for d in all_diags:
                print(d)
        print('')
    output_text = ''
    if args.format in ('mermaid', 'markdown'):
        output_text = '# Story Flow Diagram\n\n' + '\n\n'.join(all_mermaid)
    elif args.format == 'json':
        output_text = json.dumps(all_json, indent=2, ensure_ascii=False)
    if args.out:
        with open(args.out, 'w', encoding='utf-8') as f:
            f.write(output_text)
        print(f'Wrote story flow to {args.out}')
    else:
        print(output_text)

if __name__ == '__main__':
    main()
