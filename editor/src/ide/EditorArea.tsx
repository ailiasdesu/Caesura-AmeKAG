import Editor, { type OnMount } from '@monaco-editor/react'
import * as monaco from 'monaco-editor'
import { useEditor } from '../store'
import { KagLsp } from '../lib/kagLsp'
import { EngineClient } from '../lib/rpc'

export function EditorArea() {
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const setActive = useEditor((s) => s.setActive)
  const updateDoc = useEditor((s) => s.updateDoc)
  const closeDoc = useEditor((s) => s.closeDoc)

  const active = docs.find((d) => d.path === activePath) ?? null

  const handleMount: OnMount = (editor, monacoInstance) => {
    // KAG language service (Battle 2): completion / hover / diagnostics
    // bridged to the engine's declarative command contracts via /api/eval.
    const lsp = new KagLsp(new EngineClient(), monacoInstance as typeof monaco)
    lsp.register()
    editor.onDidDispose(() => lsp.dispose())
    // Ctrl+S marks the doc clean (saving via engine happens through the
    // debug/eval path in a later iteration; the store keeps dirty state).
    editor.addCommand(/* monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS */ 2049, () => {
      // placeholder: Ctrl+S is wired once the engine gains a write route
    })
  }

  return (
    <div className="editor-area">
      <div className="tab-bar" role="tablist">
        {docs.map((d) => (
          <div
            key={d.path}
            role="tab"
            className={`editor-tab ${d.path === activePath ? 'active' : ''}`}
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
              fontSize: 13,
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
