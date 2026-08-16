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

// ---------------------------------------------------------------------------
// Long-scene virtualization (performance increment): the outline flattens labels
// and items into one linear row list so the SceneOutline component can render
// only the rows near the scroll window. The window computation is a pure
// function so it is directly unit-testable without a DOM / scroll layout.
// ---------------------------------------------------------------------------

/** Fixed pixel height of a single outline row (command / text / label heading).
 *  Rows are fixed-height so the visible slice is derivable from scrollTop. */
export const OUTLINE_ROW_HEIGHT = 20

/** Scroll-window over-scan: extra rows rendered above/below the viewport so
 *  fast scrolls / partial rows never flash against an empty backdrop. */
export const OUTLINE_OVERSCAN = 4

/** Fallback viewport height (px) used when the DOM cannot report a real
 *  clientHeight (jsdom tests, SSR). Matches the .scene-tree max-height. */
export const DEFAULT_OUTLINE_VIEWPORT = 320

/** A flat outline row: either a section heading (label / prologue) or a single
 *  command / text item. Every row has a stable key for the virtual list. */
export type OutlineFlatRow =
  | { rowKind: 'section'; key: string; section: OutlineSection }
  | { rowKind: 'item'; key: string; item: OutlineItem }

/** Flatten labelled sections into one linear list of renderable rows. Item
 *  order is preserved; each label heading precedes its own items. */
export function flattenOutlineRows(sections: OutlineSection[]): OutlineFlatRow[] {
  const rows: OutlineFlatRow[] = []
  for (const section of sections) {
    rows.push({ rowKind: 'section', key: 'sec:' + section.line, section })
    for (const item of section.items) {
      rows.push({ rowKind: 'item', key: item.line + ':' + item.kind, item })
    }
  }
  return rows
}

/** Source line a flat row maps to (section headings and items both carry one). */
export function flatRowLine(row: OutlineFlatRow): number {
  return row.rowKind === 'section' ? row.section.line : row.item.line
}

/** The slice of a flat row list that is (or should be) rendered for the given
 *  scroll position. Pure and DOM-free: given a fixed row height and viewport
 *  height, the visible rows are a contiguous index range.
 *
 *  Returns:
 *  - startIndex / endIndex: inclusive range of rows to render
 *  - paddingTop / paddingBottom: pixel spacing (outside the range) that keeps
 *    the spacer-driven scroll height correct.
 */
export interface OutlineWindow {
  startIndex: number
  endIndex: number
  paddingTop: number
  paddingBottom: number
}

export interface OutlineWindowInput {
  scrollTop: number
  viewportHeight: number
  rowHeight: number
  totalRows: number
  /** Extra rows above/below the strict viewport. Defaults to OUTLINE_OVERSCAN. */
  overscan?: number
}

export function getVisibleWindow(input: OutlineWindowInput): OutlineWindow {
  const { scrollTop, viewportHeight, rowHeight, totalRows } = input
  const overscan = input.overscan ?? OUTLINE_OVERSCAN
  const last = Math.max(totalRows - 1, 0)

  if (totalRows <= 0 || viewportHeight <= 0 || rowHeight <= 0) {
    return { startIndex: 0, endIndex: last, paddingTop: 0, paddingBottom: 0 }
  }

  let startIndex = Math.floor(Math.max(0, scrollTop) / rowHeight) - overscan
  let endIndex =
    Math.floor((Math.max(0, scrollTop) + viewportHeight) / rowHeight) + overscan

  startIndex = Math.min(Math.max(0, startIndex), last)
  endIndex = Math.min(Math.max(0, endIndex), last)
  if (endIndex < startIndex) endIndex = startIndex

  const paddingTop = startIndex * rowHeight
  const paddingBottom = Math.max(0, (last - endIndex) * rowHeight)
  return { startIndex, endIndex, paddingTop, paddingBottom }
}

export interface RevealRowInput {
  /** Absolute index of the row to reveal. */
  rowIndex: number
  rowHeight: number
  viewportHeight: number
  currentScrollTop: number
  totalRows: number
}

/** The scrollTop that brings rowIndex fully into view, leaving the current
 *  scroll position untouched when the row is already visible. Clamped to the
 *  valid scroll range for the given row count and viewport. Pure. */
export function scrollTopToRevealRow(input: RevealRowInput): number {
  const { rowIndex, rowHeight, viewportHeight, currentScrollTop, totalRows } = input
  if (rowHeight <= 0 || viewportHeight <= 0) return Math.max(0, currentScrollTop)
  const maxScrollTop = Math.max(0, totalRows * rowHeight - viewportHeight)
  const top = rowIndex * rowHeight
  const bottom = top + rowHeight
  const next =
    top < currentScrollTop
      ? top
      : bottom > currentScrollTop + viewportHeight
        ? bottom - viewportHeight
        : currentScrollTop
  return Math.min(Math.max(0, next), maxScrollTop)
}

