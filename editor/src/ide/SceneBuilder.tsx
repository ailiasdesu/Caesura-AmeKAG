// Scene Builder — first step of visual scene construction.
//
// Draggable / click-to-place palette that turns demo assets and compose actions
// into KAG .ks statements inserted into the active document. Reads the active
// .ks doc from the editor store, offers the visual asset palette (from the
// static DEMO_ASSETS manifest, pending the engine /api/assets merge) and a
// dialogue compose row (+ page break).
//
// Insertion point: the generated line lands just above the live editor cursor
// (EditorArea reports it into store.editorCursor); without a tracked cursor it
// is appended at the end of the document (the store's insertAtCursor fallback).
// After every insert the panel selects the new line for the Inspector
// (setInspected) so the result is immediately inspectable.

import { useMemo, useState } from 'react'
import { useEditor } from '../store'
import { resolveInsertLine } from '../store'
import { revealEditorLine } from './EditorArea'
import {
  DEMO_ASSETS,
  buildBgLine,
  buildSpriteLine,
  buildDialogueLine,
  PAGE_BREAK_LINE,
  isBlankDialogue,
} from '../lib/sceneBuilder'

/** True when the given doc path is a .ks script. */
function isKsPath(path: string): boolean {
  return path.toLowerCase().endsWith('.ks')
}

export function SceneBuilder() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const insertAtCursor = useEditor((s) => s.insertAtCursor)
  const setInspected = useEditor((s) => s.setInspected)

  const active = useMemo(() => {
    const doc = docs.find((d) => d.path === activePath) ?? null
    if (!doc || !isKsPath(doc.path)) return null
    return doc
  }, [docs, activePath])

  // Position inputs for sprite (立绘) insertion.
  const [spriteX, setSpriteX] = useState(0)
  const [spriteY, setSpriteY] = useState(0)
  const [spriteName, setSpriteName] = useState('Hero')
  // Dialogue compose inputs.
  const [dialName, setDialName] = useState('')
  const [dialText, setDialText] = useState('')

  // Asset palette: the static demo manifest. In v1 the palette is deterministic
  // (client tests also stay stable); merging the engine's live /api/assets image
  // list on top is a follow-up increment.
  const assets = DEMO_ASSETS

  if (!active) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Scene Builder</div>
        <div className="explorer-empty">
          Open a .ks script to place backgrounds, sprites and dialogue
        </div>
      </div>
    )
  }

  const bgAssets = assets.filter((a) => a.kind === 'bg')
  const spriteAssets = assets.filter((a) => a.kind === 'sprite')

  /** Insert a freshly generated statement and select its line for the
   *  Inspector. The target line is resolved on the pre-insert content so the
   *  new statement itself is highlighted. */
  const insertStatement = (line: string) => {
    if (!active) return
    const lineIndex = resolveInsertLine(active.content, useEditor.getState().editorCursor, active.path)
    insertAtCursor(line)
    const insertedLine = lineIndex + 1
    setInspected(active.path, insertedLine)
    revealEditorLine(active.path, insertedLine)
  }

  const handleAddDialogue = () => {
    if (!active || isBlankDialogue(dialText)) return
    insertStatement(buildDialogueLine(dialName, dialText))
    setDialName('')
    setDialText('')
  }

  const handleAddPageBreak = () => {
    if (!active) return
    insertStatement(PAGE_BREAK_LINE)
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Scene Builder
        <span className="spacer" />
        <span className="scene-counts">{assets.length} assets</span>
      </div>

      <div className="builder-group-label">Backgrounds</div>
      <div className="builder-assets">
        {bgAssets.map((a) => (
          <div className="builder-asset" key={a.storage}>
            <button
              type="button"
              className="builder-insert"
              title={'Insert ' + buildBgLine(a.storage)}
              onClick={() => insertStatement(buildBgLine(a.storage))}
            >
              🖼 <span className="builder-name">{a.label}</span>
              <span className="builder-action">bg</span>
            </button>
          </div>
        ))}
      </div>
      {bgAssets.length === 0 && <div className="explorer-empty">No backgrounds</div>}

      <div className="builder-group-label">Sprites (立绘)</div>
      <div className="builder-assets">
        {spriteAssets.map((a) => (
          <div className="builder-asset" key={a.storage}>
            <button
              type="button"
              className="builder-insert"
              title={'Insert ' + buildSpriteLine(a.storage, spriteX, spriteY, spriteName)}
              onClick={() => insertStatement(buildSpriteLine(a.storage, spriteX, spriteY, spriteName))}
            >
              👤 <span className="builder-name">{a.label}</span>
              <span className="builder-action">csp</span>
            </button>
          </div>
        ))}
      </div>
      {spriteAssets.length === 0 && <div className="explorer-empty">No sprites</div>}

      <div className="builder-group-label">Sprite name</div>
      <div className="builder-pos">
        <label>
          name
          <input
            className="builder-input"
            type="text"
            value={spriteName}
            onChange={(e) => setSpriteName(e.target.value || '')}
          />
        </label>
      </div>

      <div className="builder-group-label">Position</div>
      <div className="builder-pos">
        <label>
          x
          <input
            className="builder-input"
            type="number"
            value={spriteX}
            onChange={(e) => setSpriteX(Number(e.target.value) || 0)}
          />
        </label>
        <label>
          y
          <input
            className="builder-input"
            type="number"
            value={spriteY}
            onChange={(e) => setSpriteY(Number(e.target.value) || 0)}
          />
        </label>
      </div>

      <div className="builder-group-label">Dialogue</div>
      <div className="builder-compose">
        <input
          className="builder-input"
          placeholder="name (optional)"
          value={dialName}
          onChange={(e) => setDialName(e.target.value)}
        />
        <input
          className="builder-input"
          placeholder="text…"
          value={dialText}
          onChange={(e) => setDialText(e.target.value)}
        />
        <div className="builder-compose-row">
          <button
            type="button"
            className="builder-insert"
            disabled={isBlankDialogue(dialText)}
            onClick={handleAddDialogue}
          >
            💬 Add [ch]
          </button>
          <button
            type="button"
            className="builder-insert"
            onClick={handleAddPageBreak}
          >
            ⏭ Add [p]
          </button>
        </div>
      </div>
    </div>
  )
}
