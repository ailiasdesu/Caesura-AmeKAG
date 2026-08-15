// G4 editor — scene outline panel (second increment).
//
// Store-wired wrapper around the presentational <SceneOutline />. Reads the
// active document from the editor store and, when it is a .ks script, renders
// the outline. Clicking a *label heading navigates the editor: it pushes a
// reveal request into the store (consumed by EditorArea) and directly reveals
// the line in the mounted Monaco editor, matching the SceneTree click path.
//
// For a non-.ks active doc (or no doc) it shows the library empty state, so
// this panel can be dropped into the explorer sidebar alongside SceneTree
// without disturbing non-script assets.

import { useMemo } from 'react'
import { useEditor } from '../store'
import { SceneOutline } from './SceneOutline'
import { revealEditorLine } from './EditorArea'

/** Is the given doc path a .ks script? */
function isKsPath(path: string): boolean {
  return path.toLowerCase().endsWith('.ks')
}

export function SceneOutlinePanel() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const requestReveal = useEditor((s) => s.requestReveal)
  const setInspected = useEditor((s) => s.setInspected)

  // Recompute only when the active .ks doc's identity or content changes.
  const active = useMemo(() => {
    const doc = docs.find((d) => d.path === activePath) ?? null
    if (!doc || !isKsPath(doc.path)) return null
    return doc
  }, [docs, activePath])

  const handleSelectLabel = (
    _label: string,
    line: number,
  ) => {
    if (!active) return
    requestReveal(active.path, line)
    revealEditorLine(active.path, line)
    setInspected(active.path, line)
  }

  if (!active) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Scene Outline</div>
        <div className="explorer-empty">Open a .ks script to see its outline</div>
      </div>
    )
  }

  return <SceneOutline source={active.content} onSelectLabel={handleSelectLabel} />
}
