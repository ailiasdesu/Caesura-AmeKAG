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
import { buildLabelJumpSnippet, parseJumpResult } from '../lib/engineJump'
import type { EngineClient } from '../lib/rpc'

/** Is the given doc path a .ks script? */
function isKsPath(path: string): boolean {
  return path.toLowerCase().endsWith('.ks')
}

interface SceneOutlinePanelProps {
  /** Optional engine RPC client. When provided AND the engine is connected,
   *  the outline's per-label jump drives the running KAG scene to that
   *  label via /api/eval; otherwise the jump action degrades to the
   *  in-editor reveal (same as the primary label click). */
  client?: EngineClient
}

export function SceneOutlinePanel({ client }: SceneOutlinePanelProps) {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const engineConnected = useEditor((s) => s.engineConnected)
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

  // Non-blocking engine jump: issue the eval snippet against the live KAG
  // ctx (_G._CAESURA_CTX). Always reveal the line in-editor first, so the
  // affordance behaves identically whether or not the engine is reachable;
  // the engine call is fire-and-forget and swallows transport/label errors.
  const handleJumpToLabel = (label: string, line: number) => {
    if (!active) return
    handleSelectLabel(label, line)
    if (!engineConnected || !client) return
    void evalRawGuarded(client, label)
  }

  if (!active) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Scene Outline</div>
        <div className="explorer-empty">Open a .ks script to see its outline</div>
      </div>
    )
  }

  return (
    <SceneOutline
      source={active.content}
      onSelectLabel={handleSelectLabel}
      onJumpToLabel={handleJumpToLabel}
    />
  )
}

/** Issue the engine label-jump eval and swallow all failures (no crash). */
async function evalRawGuarded(
  client: EngineClient,
  label: string,
): Promise<void> {
  try {
    const result = await client.evalRaw(buildLabelJumpSnippet(label))
    parseJumpResult(result)
  } catch {
    // Transport / engine rejection must never surface into the click.
  }
}
