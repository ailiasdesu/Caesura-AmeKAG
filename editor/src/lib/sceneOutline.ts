// G4 editor — scene outline parser (first increment).
//
// Reads a .ks scene document and produces a structured outline: *labels as
// section headings, [command ...] tags as command rows (command name + key
// params), and bare text lines as content. Unlike the flat scene-element
// stream used by SceneTree/TimelineView, this parser *keeps* text lines
// (dialogue / narration) so the outline can show the full scene flow.
//
// Deterministic and tolerant: unknown/malformed lines degrade to text or a
// minimal command row instead of throwing (no full KAG grammar needed).

import type { OutlineItem } from '../ide/sceneOutlineTypes'
import type { OutlineSection } from '../ide/sceneOutlineTypes'

/** Extract \`key="value"\` / \`key=number\` / bare-flag parameter pairs from a
 *  tag body. Same behaviour as SceneTree's parseTagParams (kept local so the
 *  parser stays free of the monaco-importing component graph). Tolerant of
 *  malformed bodies (returns {}). */
export function parseTagParams(body: string): Record<string, string> {
  const params: Record<string, string> = {}
  const re = /([\w_]+)\s*=\s*"((?:[^"\\]|\\.)*)"?|([\w_]+)\s*=\s*([\w.+-]+)|([\w_]+)/g
  let m: RegExpExecArray | null
  while ((m = re.exec(body)) !== null) {
    if (m[1] !== undefined && m[2] !== undefined) params[m[1]] = m[2]
    else if (m[3] !== undefined && m[4] !== undefined) params[m[3]] = m[4]
    else if (m[5] !== undefined) params[m[5]] = 'true'
  }
  return params
}

/** Parse a `*name` label heading line. Returns the name (no '*') or null. */
function matchLabel(trimmed: string): string | null {
  const m = trimmed.match(/^\*([\w_]+)/)
  return m ? m[1] : null
}

/** Parse a `[command ...]` tag line inta a command outline item, or null
 *  when the line is not a tag. Unclosed/malformed tags that still carry a
 *  command word are tolerated; a bracket with no command degrades to null
 *  (the caller then treats it as text content). */
function matchCommand(trimmed: string): OutlineItem | null {
  if (!trimmed.startsWith('[')) return null
  const head = trimmed.match(/^\[([\w_]+)/)
  if (!head) return null
  const cmd = head[1]
  const body = trimmed.slice(trimmed.indexOf(cmd) + cmd.length)
  return { kind: 'command', line: 0, cmd, params: parseTagParams(body) }
}

/** Parse a .ks document into a flat listed sequence of outline items
 *  (labels, commands with trimmed key params, and text content lines).
 *  Pure; tolerant of empty/whitespace/comment-only sources (yields []). */
export function parseSceneOutline(source: string): OutlineItem[] {
  const items: OutlineItem[] = []
  const lines = source.split('\n')
  for (let i = 0; i < lines.length; i++) {
    const trimmed = (lines[i] ?? '').trim()
    if (!trimmed) continue
    if (trimmed.startsWith(';')) continue

    const label = matchLabel(trimmed)
    if (label !== null) {
      items.push({ kind: 'label', line: i + 1, name: label })
      continue
    }

    const cmd = matchCommand(trimmed)
    if (cmd !== null) {
      items.push({ ...cmd, line: i + 1 })
      continue
    }

    items.push({ kind: 'text', line: i + 1, content: trimmed })
  }
  return items
}

/** Group flat outline items into labelled sections (one per *label). Items
 *  before the first label form the prologue section (label === null). */
export function buildOutlineSections(items: OutlineItem[]): OutlineSection[] {
  const sections: OutlineSection[] = []
  let current: OutlineSection | null = null
  for (const item of items) {
    if (item.kind === 'label') {
      current = { label: item.name, line: item.line, items: [] }
      sections.push(current)
      continue
    }
    if (!current) {
      current = { label: null, line: item.line, items: [] }
      sections.push(current)
    }
    current.items.push(item)
  }
  return sections
}
