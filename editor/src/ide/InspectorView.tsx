import { useMemo } from 'react'
import { useEditor } from '../store'
import { parseSceneElements } from './SceneTree'

// G4: scene-element inspector. Reads the currently inspected element
// (path + line, set by SceneTree clicks) and renders its full parameter
// table — the detail line only shows the first storage/file/text/name
// param, while the inspector shows every key/value pair.
export function InspectorView() {
  const docs = useEditor((s) => s.docs)
  const inspected = useEditor((s) => s.inspected)

  const doc = inspected ? docs.find((d) => d.path === inspected.path) ?? null : null
  const element = useMemo(() => {
    if (!doc || !inspected) return null
    return (
      parseSceneElements(doc.content).find((e) => e.line === inspected.line) ?? null
    )
  }, [doc, inspected])

  if (!inspected) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Inspector</div>
        <div className="explorer-empty">Click a scene element to inspect it</div>
      </div>
    )
  }

  if (!doc || !element) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Inspector</div>
        <div className="explorer-empty">Element not found (document closed or line removed)</div>
      </div>
    )
  }

  const params = Object.entries(element.params)
  const lineText = doc.content.split('\n')[element.line - 1] ?? ''

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Inspector
        <span className="spacer" />
        <span className="scene-counts">L{element.line}</span>
      </div>
      <div className="inspector-body">
        <div className="state-row">
          <span>type</span>
          <b>{element.type}</b>
        </div>
        <div className="state-row">
          <span>command</span>
          <b>{element.text}</b>
        </div>
        {params.length === 0 ? (
          <div className="state-row">
            <span>params</span>
            <b>—</b>
          </div>
        ) : (
          params.map(([k, v]) => (
            <div className="state-row" key={k}>
              <span>{k}</span>
              <b title={v}>{v}</b>
            </div>
          ))
        )}
        <div className="inspector-source" title={lineText}>
          {lineText || '(blank line)'}
        </div>
      </div>
    </div>
  )
}
