import { useMemo, useState } from 'react'
import { useEditor } from '../store'
import { revealEditorLine } from './EditorArea'
import { parseSceneElements, type SceneElement } from './SceneTree'
import { sceneMatchesDoc } from '../lib/enginePosition'

// G4: timeline view — the scene element stream grouped into labelled
// sections (one per *label), with per-type filtering. Pure section
// builder so the grouping logic is unit-testable without DOM.

export interface TimelineSection {
  label: string | null
  line: number
  elements: SceneElement[]
}

export type SceneFilter = 'all' | SceneElement['type'] | 'text'

/** Group parsed scene elements into sections by *label boundaries. */
export function buildTimelineSections(source: string): TimelineSection[] {
  const sections: TimelineSection[] = []
  let current: TimelineSection | null = null
  for (const e of parseSceneElements(source)) {
    if (e.type === 'label') {
      current = { label: e.text.slice(1), line: e.line, elements: [] }
      sections.push(current)
      continue
    }
    if (!current) {
      current = { label: null, line: e.line, elements: [] }
      sections.push(current)
    }
    current.elements.push(e)
  }
  return sections
}

/** A bare dialog / content line, which parseSceneElements does not emit. */
export interface TimelineTextLine {
  line: number
  text: string
}

/** Extract bare text (non-tag, non-label, non-comment, non-blank) lines. */
export function collectTextLines(source: string): TimelineTextLine[] {
  const out: TimelineTextLine[] = []
  const lines = source.split('\n')
  for (let i = 0; i < lines.length; i++) {
    const trimmed = lines[i].trim()
    if (!trimmed) continue
    if (trimmed.startsWith(';')) continue
    if (/^\*[\w_]+/.test(trimmed)) continue
    if (/^\[[\w_]+/.test(trimmed)) continue
    out.push({ line: i + 1, text: trimmed })
  }
  return out
}

/** A timeline section augmented with the bare text lines that fall inside it. */
export interface TimelineSectionAll {
  label: string | null
  line: number
  elements: SceneElement[]
  texts: TimelineTextLine[]
}

/** Build grouped sections including both the element stream and bare text
 *  lines, ordered by source line within each section. */
export function buildTimelineSectionRows(source: string): TimelineSectionAll[] {
  const byLine = new Map<number, SceneElement>()
  for (const e of parseSceneElements(source)) byLine.set(e.line, e)
  const textByLine = new Map<number, string>()
  for (const t of collectTextLines(source)) textByLine.set(t.line, t.text)

  const sections: TimelineSectionAll[] = []
  let current: TimelineSectionAll | null = null
  const lineNumbers = Array.from(
    new Set([...byLine.keys(), ...textByLine.keys()]),
  ).sort((a, b) => a - b)

  for (const line of lineNumbers) {
    const el = byLine.get(line)
    if (el) {
      if (el.type === 'label') {
        current = { label: el.text.slice(1), line: el.line, elements: [], texts: [] }
        sections.push(current)
        continue
      }
      if (!current) {
        current = { label: null, line: el.line, elements: [], texts: [] }
        sections.push(current)
      }
      current.elements.push(el)
      continue
    }
    const text = textByLine.get(line)
    if (text !== undefined) {
      if (!current) {
        current = { label: null, line: line, elements: [], texts: [] }
        sections.push(current)
      }
      current.texts.push({ line: line, text: text })
    }
  }
  return sections
}

/** The source lines of a section's content rows in document order, merging
 *  elements and bare-text lines by source line. Pure. */
function sectionRowLines(s: TimelineSectionAll): number[] {
  const items: number[] = [...s.elements.map((e) => e.line), ...s.texts.map((t) => t.line)]
  return items.sort((a, b) => a - b)
}

/** Resolve an engine token index to a source line. Each *label heading counts
 *  as one token at its own line; every element and bare-text line is one
 *  token in document order. The prologue contributes only its content rows
 *  (no synthetic heading). Returns null when the token is out of range. Pure. */
export function timelineTokenToLine(source: string, tokenIndex: number): number | null {
  const sections = buildTimelineSectionRows(source)
  const rows: number[] = []
  for (const s of sections) {
    if (s.label !== null) rows.push(s.line)
    for (const line of sectionRowLines(s)) rows.push(line)
  }
  const idx = tokenIndex - 1
  if (!Number.isFinite(tokenIndex) || idx < 0 || idx >= rows.length) return null
  return rows[idx]
}

/** Non-heading rows (elements + texts) in a section. */
function sectionRowCount(s: TimelineSectionAll): number {
  return s.elements.length + s.texts.length
}

const FILTERS: { id: SceneFilter; label: string }[] = [
  { id: 'all', label: 'All' },
  { id: 'bg', label: 'BG' },
  { id: 'fg', label: 'FG' },
  { id: 'ch', label: 'CH' },
  { id: 'audio', label: 'Audio' },
  { id: 'label', label: 'Labels' },
  { id: 'text', label: 'Text' },
]

interface NavRow {
  line: number
  kind: 'section' | 'line'
}

export function TimelineView() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const requestReveal = useEditor((s) => s.requestReveal)
  const setInspected = useEditor((s) => s.setInspected)
  const inspected = useEditor((s) => s.inspected)
  const engineCmd = useEditor((s) => s.engineCmd)
  const engineToken = useEditor((s) => s.engineToken)
  const engineScene = useEditor((s) => s.engineScene)
  const engineConnected = useEditor((s) => s.engineConnected)
  const [filter, setFilter] = useState<SceneFilter>('all')
  const [focusedIdx, setFocusedIdx] = useState<number | null>(null)

  const active = docs.find((d) => d.path === activePath) ?? null

  const sections = useMemo(
    () => (active ? buildTimelineSectionRows(active.content) : []),
    [active],
  )
  const sceneMatches = Boolean(
    active && engineConnected && sceneMatchesDoc(engineScene, active.path),
  )
  const engineLine = useMemo(() => {
    if (!sceneMatches || !active) return null
    if (typeof engineToken !== 'number' || engineToken < 1) return null
    return timelineTokenToLine(active.content, engineToken)
  }, [sceneMatches, engineToken, active])

  if (!active) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Timeline</div>
        <div className="explorer-empty">Open a .ks script to see its timeline</div>
      </div>
    )
  }

  const visible = sections
    .map((s) => ({
      ...s,
      elements:
        filter === 'all' || filter === 'label'
          ? s.elements
          : filter === 'text'
            ? []
            : s.elements.filter((e) => e.type === filter),
      texts: filter === 'all' || filter === 'text' ? s.texts : [],
    }))
    .filter((s) => sectionRowCount(s) > 0 || (filter === 'label' && s.label !== null))

  const navRows: NavRow[] = []
  for (const s of visible) {
    navRows.push({ line: s.line, kind: 'section' })
    for (const line of sectionRowLines(s)) navRows.push({ line, kind: 'line' })
  }

  const jump = (line: number) => {
    requestReveal(active.path, line)
    revealEditorLine(active.path, line)
    setInspected(active.path, line)
  }

  const isInspected = (line: number) =>
    inspected?.path === active.path && inspected.line === line

  const revealMove = (idx: number) => {
    if (idx < 0 || idx >= navRows.length) return
    setFocusedIdx(idx)
    requestReveal(active.path, navRows[idx].line)
    revealEditorLine(active.path, navRows[idx].line)
  }

  const handleKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    if (navRows.length === 0) return
    const base = focusedIdx ?? -1
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      revealMove(base < 0 ? 0 : Math.min(base + 1, navRows.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      revealMove(base < 0 ? 0 : Math.max(base - 1, 0))
    } else if (e.key === 'Home') {
      e.preventDefault()
      revealMove(0)
    } else if (e.key === 'End') {
      e.preventDefault()
      revealMove(navRows.length - 1)
    } else if (e.key === 'Enter') {
      if (focusedIdx !== null) {
        e.preventDefault()
        jump(navRows[focusedIdx].line)
      }
    }
  }

  let navCounter = 0
  const focusClass = (idx: number | undefined) =>
    idx !== undefined && idx === focusedIdx ? ' timeline-focused' : ''

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Timeline
        <span className="spacer" />
        <span className="scene-counts">{visible.length} sec</span>
      </div>
      {engineConnected && engineCmd && (
        <div className="timeline-exec" title="Engine execution position">
          <span className="exec-live">▶</span>
          <span className="exec-cmd">{engineCmd}</span>
          <span className="spacer" />
          <span className="scene-counts">token {engineToken}</span>
        </div>
      )}
      <div className="timeline-filters">
        {FILTERS.map((f) => (
          <button
            key={f.id}
            className={'timeline-chip' + (filter === f.id ? ' active' : '')}
            onClick={() => setFilter(f.id)}
          >
            {f.label}
          </button>
        ))}
      </div>
      <div
        className="scene-tree timeline-body timeline-focusable"
        tabIndex={0}
        onKeyDown={handleKeyDown}
        data-testid="timeline-body"
      >
        {sections.length === 0 ? (
          <div className="explorer-empty">No timeline elements (empty script)</div>
        ) : visible.length === 0 ? (
          <div className="explorer-empty">No matching elements</div>
        ) : (
          visible.map((s) => {
            const headingIdx = navCounter++
            const headingActive = engineLine === s.line
            const headingClass =
              'timeline-section-title' +
              (headingActive ? ' outline-current' : '') +
              (isInspected(s.line) ? ' inspected' : '') +
              focusClass(headingIdx)
            const rows = sectionRowLines(s)
            return (
              <div className="timeline-section" key={s.line}>
                <div
                  className={headingClass}
                  onClick={() => jump(s.line)}
                  title={s.label ? 'label *' + s.label : 'prologue'}
                  role="button"
                  tabIndex={-1}
                  aria-pressed={isInspected(s.line)}
                >
                  {s.label ? '* ' + s.label : '(prologue)'}
                  <span className="spacer" />
                  <span className="scene-counts">
                    L{s.line} · {sectionRowCount(s)}
                  </span>
                </div>
                {rows.map((line) => {
                  const el = s.elements.find((e) => e.line === line)
                  const text = el ? null : s.texts.find((t) => t.line === line)
                  const bodyIdx = navCounter++
                  const bodyActive = engineLine === line
                  let cls = 'scene-el'
                  let title: string
                  let inner: JSX.Element
                  if (el) {
                    cls += ' scene-' + el.type
                    title = 'line ' + line + ' — ' + el.detail
                    inner = (
                      <>
                        <span className="scene-line">{line}</span>
                        <span className="scene-text">{el.text}</span>
                        {el.detail && <span className="scene-detail">{el.detail}</span>}
                      </>
                    )
                  } else if (text) {
                    cls += ' scene-text-row'
                    title = 'line ' + line + ' — ' + text.text
                    inner = (
                      <>
                        <span className="scene-line">{line}</span>
                        <span className="scene-text scene-text-content">{text.text}</span>
                      </>
                    )
                  } else {
                    return null
                  }
                  cls +=
                    (bodyActive ? ' outline-current' : '') +
                    (isInspected(line) ? ' inspected' : '') +
                    focusClass(bodyIdx)
                  return (
                    <button
                      key={'r' + line}
                      className={cls}
                      title={title}
                      onClick={() => jump(line)}
                      onMouseEnter={() => setFocusedIdx(bodyIdx)}
                      aria-pressed={isInspected(line)}
                    >
                      {inner}
                    </button>
                  )
                })}
              </div>
            )
          })
        )}
      </div>
    </div>
  )
}
