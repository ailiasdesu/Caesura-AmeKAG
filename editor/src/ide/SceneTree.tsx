// Scene tree panel — scene element outline of the active .ks document.
//
// Groups parsed elements by type into collapsible sections (round 90+ depth):
//  - each type group is a <section role="group"> with a toggle <button>
//    carrying aria-expanded / aria-controls, so the panel stays keyboard
//    navigable and screen-reader friendly;
//  - clicking an element row navigates: push a reveal request into the store
//    (consumed by EditorArea), directly reveal the line in the mounted Monaco
//    editor, and select the element for the G4 inspector (double write);
//  - selection stays reactive to the store, so an external inspected change
//    (e.g. from the inspector or outline panel) re-highlights the row.

import { useEffect, useMemo, useState } from 'react'
import { useEditor } from '../store'
import { revealEditorLine } from './EditorArea'

export interface SceneElement {
  line: number
  type: 'label' | 'bg' | 'fg' | 'ch' | 'audio' | 'other'
  text: string
  detail: string
  /** Tag parameter table (key="value" / key=number pairs), G4 inspector. */
  params: Record<string, string>
}

/** Stable display order for type groups (labels first, then visuals, dialog,
 *  audio, and anything unrecognised). */
export const TYPE_ORDER: SceneElement['type'][] = ['label', 'bg', 'fg', 'ch', 'audio', 'other']

const TYPE_LABELS: Record<SceneElement['type'], string> = {
  label: 'Labels',
  bg: 'Backgrounds',
  fg: 'Sprites',
  ch: 'Dialogue',
  audio: 'Audio',
  other: 'Other',
}

/** Extract `key="value"` / `key=number` / bare-flag parameter pairs from a
 *  tag body (G4 inspector). Quoted values win over bare tokens; a quoted
 *  value may contain '='. Tolerant of malformed bodies (returns {}). */
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

/** Parse a .ks document into a scene element tree (Battle 4b).
 *  Pure function — regex over the source; tolerant of malformed lines. */
export function parseSceneElements(source: string): SceneElement[] {
  const out: SceneElement[] = []
  const lines = source.split('\n')
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i]
    const trimmed = line.trim()
    if (!trimmed) continue
    if (trimmed.startsWith(';')) continue

    // *label
    const label = trimmed.match(/^\*([\w_]+)/)
    if (label) {
      out.push({ line: i + 1, type: 'label', text: `*${label[1]}`, detail: '', params: {} })
      continue
    }

    // [tag ...]
    const tag = trimmed.match(/^\[([\w_]+)\s*(.*?)\]?$/)
    if (tag) {
      const cmd = tag[1]
      const rest = tag[2] ?? ''
      const storage = rest.match(/(?:storage|file|text|name)\s*=\s*"([^"]*)"/)
      const detail = storage ? storage[1] : rest.length > 30 ? rest.slice(0, 30) + '…' : rest
      let type: SceneElement['type'] = 'other'
      if (cmd === 'bg' || cmd === 'image') type = 'bg'
      else if (cmd === 'fg' || cmd === 'chara_show') type = 'fg'
      else if (cmd === 'ch' || cmd === 'text') type = 'ch'
      else if (cmd === 'playbgm' || cmd === 'playse' || cmd === 'play') type = 'audio'
      out.push({ line: i + 1, type, text: `[${cmd}]`, detail, params: parseTagParams(rest) })
    }
  }
  return out
}

const ICONS: Record<SceneElement['type'], string> = {
  label: '🏷',
  bg: '🖼',
  fg: '👤',
  ch: '💬',
  audio: '🎵',
  other: '▸',
}

/** Which of the per-type groups are currently expanded (default all open). */
type Expansion = Record<SceneElement['type'], boolean>

export function SceneTree() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const requestReveal = useEditor((s) => s.requestReveal)
  const inspected = useEditor((s) => s.inspected)
  const setInspected = useEditor((s) => s.setInspected)

  const active = docs.find((d) => d.path === activePath) ?? null
  const elements = useMemo(
    () => (active ? parseSceneElements(active.content) : []),
    [active],
  )

  const [expanded, setExpanded] = useState<Expansion>(() =>
    Object.fromEntries(TYPE_ORDER.map((t) => [t, true])) as Expansion,
  )
  const toggleGroup = (type: SceneElement['type']) =>
    setExpanded((prev) => ({ ...prev, [type]: !prev[type] }))

  useEffect(() => {
    if (active) requestReveal(active.path, 1)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activePath])

  if (!active) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Scene Tree</div>
        <div className="explorer-empty">Open a .ks script to see its scene</div>
      </div>
    )
  }

  const counts = { label: 0, bg: 0, fg: 0, ch: 0, audio: 0, other: 0 }
  for (const e of elements) counts[e.type]++

  const grouped = TYPE_ORDER.filter((t) => counts[t] > 0).map((t) => ({
    type: t,
    label: TYPE_LABELS[t],
    icon: ICONS[t],
    items: elements.filter((e) => e.type === t),
  }))

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Scene Tree
        <span className="spacer" />
        <span className="scene-counts">
          {counts.label}🏷 {counts.bg}🖼 {counts.ch}💬
        </span>
      </div>
      <div className="scene-tree">
        {grouped.map((g) => (
          <section
            key={g.type}
            className={`scene-group scene-group-${g.type}`}
            aria-label={g.label}
          >
            <button
              type="button"
              className="scene-group-header"
              aria-expanded={expanded[g.type]}
              aria-controls={`scene-group-${g.type}`}
              onClick={() => toggleGroup(g.type)}
            >
              {expanded[g.type] ? '▾' : '▸'}
              <span className="scene-icon">{g.icon}</span>
              <span className="scene-text">{g.label}</span>
              <span className="scene-count">{g.items.length}</span>
            </button>
            {expanded[g.type] && (
              <div id={`scene-group-${g.type}`}>
                {g.items.map((e) => (
                  <button
                    key={e.line}
                    aria-pressed={inspected?.path === active.path && inspected.line === e.line}
                    className={`scene-el scene-${e.type}${inspected?.path === active.path && inspected.line === e.line ? ' inspected' : ''}`}
                    title={`line ${e.line} — ${e.detail}`}
                    onClick={() => {
                      requestReveal(active.path, e.line)
                      revealEditorLine(active.path, e.line)
                      setInspected(active.path, e.line)
                    }}
                  >
                    <span className="scene-icon">{ICONS[e.type]}</span>
                    <span className="scene-text">{e.text}</span>
                    {e.detail && <span className="scene-detail">{e.detail}</span>}
                    <span className="scene-line">{e.line}</span>
                  </button>
                ))}
              </div>
            )}
          </section>
        ))}
        {elements.length === 0 && (
          <div className="explorer-empty">No scene elements found</div>
        )}
      </div>
    </div>
  )
}