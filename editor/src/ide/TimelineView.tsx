import { useMemo, useState } from 'react'
import { useEditor } from '../store'
import { revealEditorLine } from './EditorArea'
import { parseSceneElements, type SceneElement } from './SceneTree'

// G4: timeline view — the scene element stream grouped into labelled
// sections (one per *label), with per-type filtering. Pure section
// builder so the grouping logic is unit-testable without DOM.

export interface TimelineSection {
  /** Section label text (without the leading '*'), or null for the
   *  prologue before the first label. */
  label: string | null
  /** Line of the label token (or the first element for the prologue). */
  line: number
  elements: SceneElement[]
}

export type SceneFilter = 'all' | SceneElement['type']

/** Group parsed scene elements into sections by *label boundaries.
 *  Pure function — tolerant of empty sources and label-less scripts. */
export function buildTimelineSections(source: string): TimelineSection[] {
  const sections: TimelineSection[] = []
  let current: TimelineSection | null = null
  for (const e of parseSceneElements(source)) {
    if (e.type === 'label') {
      current = {
        label: e.text.slice(1), // strip '*'
        line: e.line,
        elements: [],
      }
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

const FILTERS: { id: SceneFilter; label: string }[] = [
  { id: 'all', label: 'All' },
  { id: 'bg', label: 'BG' },
  { id: 'fg', label: 'FG' },
  { id: 'ch', label: 'CH' },
  { id: 'audio', label: 'Audio' },
  { id: 'label', label: 'Labels' },
]

export function TimelineView() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const requestReveal = useEditor((s) => s.requestReveal)
  const setInspected = useEditor((s) => s.setInspected)
  const [filter, setFilter] = useState<SceneFilter>('all')

  const active = docs.find((d) => d.path === activePath) ?? null
  const sections = useMemo(
    () => (active ? buildTimelineSections(active.content) : []),
    [active],
  )

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
      elements: filter === 'all' ? s.elements : s.elements.filter((e) => e.type === filter),
    }))
    .filter((s) => s.elements.length > 0 || (filter === 'label' && s.label !== null))

  const jump = (line: number) => {
    requestReveal(active.path, line)
    revealEditorLine(active.path, line)
    setInspected(active.path, line)
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Timeline
        <span className="spacer" />
        <span className="scene-counts">{visible.length} sec</span>
      </div>
      <div className="timeline-filters">
        {FILTERS.map((f) => (
          <button
            key={f.id}
            className={`timeline-chip ${filter === f.id ? 'active' : ''}`}
            onClick={() => setFilter(f.id)}
          >
            {f.label}
          </button>
        ))}
      </div>
      <div className="scene-tree timeline-body">
        {visible.length === 0 && (
          <div className="explorer-empty">No matching elements</div>
        )}
        {visible.map((s) => (
          <div className="timeline-section" key={s.line}>
            <div
              className="timeline-section-title"
              onClick={() => jump(s.line)}
              title={s.label ? `label *${s.label}` : 'prologue'}
            >
              {s.label ? `* ${s.label}` : '(prologue)'}
              <span className="spacer" />
              <span className="scene-counts">L{s.line} · {s.elements.length}</span>
            </div>
            {s.elements.map((e) => (
              <button
                key={e.line}
                className={`scene-el scene-${e.type}`}
                title={`line ${e.line} — ${e.detail}`}
                onClick={() => jump(e.line)}
              >
                <span className="scene-line">{e.line}</span>
                <span className="scene-text">{e.text}</span>
                {e.detail && <span className="scene-detail">{e.detail}</span>}
              </button>
            ))}
          </div>
        ))}
      </div>
    </div>
  )
}
