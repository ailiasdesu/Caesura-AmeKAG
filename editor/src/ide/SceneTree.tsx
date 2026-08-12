import { useEffect, useMemo } from 'react'
import { useEditor } from '../store'
import { revealEditorLine } from './EditorArea'

interface SceneElement {
  line: number
  type: 'label' | 'bg' | 'fg' | 'ch' | 'audio' | 'other'
  text: string
  detail: string
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
      out.push({ line: i + 1, type: 'label', text: `*${label[1]}`, detail: '' })
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
      out.push({ line: i + 1, type, text: `[${cmd}]`, detail })
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

export function SceneTree() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const requestReveal = useEditor((s) => s.requestReveal)

  const active = docs.find((d) => d.path === activePath) ?? null
  const elements = useMemo(
    () => (active ? parseSceneElements(active.content) : []),
    [active],
  )

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
        {elements.map((e, idx) => (
          <button
            key={idx}
            className={`scene-el scene-${e.type}`}
            title={`line ${e.line} — ${e.detail}`}
            onClick={() => {
              requestReveal(active.path, e.line)
              revealEditorLine(active.path, e.line)
            }}
          >
            <span className="scene-icon">{ICONS[e.type]}</span>
            <span className="scene-text">{e.text}</span>
            {e.detail && <span className="scene-detail">{e.detail}</span>}
            <span className="scene-line">{e.line}</span>
          </button>
        ))}
        {elements.length === 0 && (
          <div className="explorer-empty">No scene elements found</div>
        )}
      </div>
    </div>
  )
}
