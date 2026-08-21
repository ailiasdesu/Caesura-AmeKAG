import Editor, { type OnMount } from '@monaco-editor/react'
import * as monaco from 'monaco-editor'
import { useEffect, useRef } from 'react'
import { useEditor } from '../store'
import { KagLsp } from '../lib/kagLsp'
import { EngineClient } from '../lib/rpc'
import { parseAssetDrop } from '../lib/assetDrop'

// Battle 4b: module-level registry so the SceneTree (outside the Monaco
// editor tree) can ask the mounted editor to reveal a line.
type StandaloneEditor = monaco.editor.IStandaloneCodeEditor
const editorRegistry = new Map<string, StandaloneEditor>()
export function registerEditor(path: string, ed: StandaloneEditor): void {
  editorRegistry.set(path, ed)
}
export function unregisterEditor(path: string, ed: StandaloneEditor): void {
  if (editorRegistry.get(path) === ed) editorRegistry.delete(path)
}
export function revealEditorLine(path: string, line: number): void {
  const ed = editorRegistry.get(path)
  if (ed) {
    ed.revealLineInCenter(Math.max(1, line))
    ed.setPosition({ lineNumber: Math.max(1, line), column: 1 })
    ed.focus()
  }
}

export function EditorArea() {

  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const setActive = useEditor((s) => s.setActive)
  const updateDoc = useEditor((s) => s.updateDoc)
  const closeDoc = useEditor((s) => s.closeDoc)
  const openDoc = useEditor((s) => s.openDoc)
  const setCursor = useEditor((s) => s.setCursor)
  const revealRequest = useEditor((s) => s.revealRequest)
  const lastReveal = useRef(0)
  // Layer "settings": Monaco font size + line-number gutter toggle.
  const fontSize = useEditor((s) => s.settings.fontSize)
  const showLineNumbers = useEditor((s) => s.settings.showLineNumbers)

  const active = docs.find((d) => d.path === activePath) ?? null

  // Scene-tree jump: consume the reveal request
  useEffect(() => {
    if (!revealRequest) return
    if (revealRequest.nonce === lastReveal.current) return
    lastReveal.current = revealRequest.nonce
    revealEditorLine(revealRequest.path, revealRequest.line)
  }, [revealRequest])

  const handleMount: OnMount = (editor, monacoInstance) => {
    // KAG language service (Battle 2): completion / hover / diagnostics
    // bridged to the engine's declarative command contracts via /api/eval.
    const lsp = new KagLsp(new EngineClient(), monacoInstance as typeof monaco)
    lsp.register()
    // Battle 4b: register the editor for scene-tree jumps
    if (active) registerEditor(active.path, editor)
    // Scene Builder: report the live cursor (0-based line → 1-based) so
    // generated lines land at the user's insertion point.
    const activeRef = { current: active }
    editor.onDidChangeCursorPosition((e) => {
      const a = activeRef.current
      if (!a) return
      setCursor({
        path: a.path,
        line: e.position.lineNumber,
        column: e.position.column,
      })
    })
    editor.onDidDispose(() => {
      lsp.dispose()
      if (active) unregisterEditor(active.path, editor)
      setCursor(null)
    })
    // Ctrl+S = CtrlCmd(2048) | KeyS(49) = 2097. This was previously a
    // hardcoded 2049 (= CtrlCmd | Backspace), which — despite the comment
    // claiming otherwise — is not the S key. Placeholder: the command body is
    // wired once the engine gains a write route (the store keeps dirty state).
    editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
      // placeholder: Ctrl+S is wired once the engine gains a write route
    })
  }

  // Asset drag-and-drop (Sprint 3b): ExplorerView drops
  // application/x-caesura-asset {path,type}. Scripts open as docs;
  // image/audio show a status hint (no media server in the engine).
  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault()
    const asset = parseAssetDrop(e.dataTransfer.getData('application/x-caesura-asset'))
    if (!asset) return
    const { path, type } = asset
    if (type === 'script') {
      setActive(path)
      openDoc({
        path,
        name: path.split('/').pop() ?? path,
        language: path.endsWith('.ks') ? 'kag' : 'lua',
        content: '',
        dirty: false,
      })
    } else {
      // image/audio: click-to-place is wired in the Visual/Scene views;
      // dropping a non-script here just nudges the user.
      setCursor({ path, line: 1, column: 1 })  // reuse cursor for a hint area
    }
  }
  const handleDragOver = (e: React.DragEvent) => {
    if (e.dataTransfer.types.includes('application/x-caesura-asset')) {
      e.preventDefault()
    }
  }

  return (
    <div className="editor-area" onDrop={handleDrop} onDragOver={handleDragOver}>
      <div className="tab-bar" role="tablist">
        {docs.map((d) => (
          <div
            key={d.path}
            role="tab"
            className={"editor-tab " + (d.path === activePath ? 'active' : '')}
            onClick={() => setActive(d.path)}
            title={d.path}
          >
            <span className="tab-icon">{d.language === 'kag' ? '📜' : '📄'}</span>
            <span className="tab-name">{d.name}</span>
            {d.dirty && <span className="dirty-dot">●</span>}
            <button
              className="tab-close"
              onClick={(e) => {
                e.stopPropagation()
                closeDoc(d.path)
              }}
            >
              ✕
            </button>
          </div>
        ))}
        {docs.length === 0 && (
          <div className="tab-empty">Open a script from the Explorer</div>
        )}
      </div>

      <div className="editor-host">
        {active ? (
          <Editor
            height="100%"
            language={active.language}
            path={active.path}
            value={active.content}
            theme="vs-dark"
            onMount={handleMount}
            onChange={(v) => {
              if (typeof v === 'string') updateDoc(active.path, v)
            }}
            options={{
              fontSize,
              lineNumbers: showLineNumbers ? 'on' : 'off',
              minimap: { enabled: false },
              scrollBeyondLastLine: false,
              wordWrap: 'on',
              tabSize: 2,
              renderWhitespace: 'selection',
              bracketPairColorization: { enabled: true },
            }}
          />
        ) : (
          <div className="editor-welcome">
            <h2>Caesura Editor</h2>
            <p>
              Open a <code>.ks</code> script from the Explorer to edit scenes.
              Use <strong>Run and Debug</strong> to drive the engine, and{' '}
              <strong>Visual Preview</strong> for live frames.
            </p>
          </div>
        )}
      </div>
    </div>
  )
}
