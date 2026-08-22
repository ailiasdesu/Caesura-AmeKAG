import { useState } from 'react'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

/** Defaults mirror the engine's POST /api/build fallbacks (EditorServer.cpp). */
const DEFAULT_OUTPUT = 'build/game.carc'
const DEFAULT_KEY = 'build/game.key'

/**
 * Build Manager panel (Sprint 5, task book §Phase1).
 *
 * CARC section drives POST /api/build via EngineClient.buildCarc().
 * The Web Package section is honest by design: web-site packaging exists only
 * as the repository script scripts/package_game.sh — the engine exposes no
 * HTTP endpoint for it, so the panel surfaces the command text instead of
 * pretending one-click support (task book §18.4: never fake support).
 * Running a scene stays in the Debug panel ("Run Current Scene") and is not
 * duplicated here.
 */
export function BuildManagerView({ client }: Props) {
  const setSideView = useEditor((s) => s.setSideView)

  const [outputPath, setOutputPath] = useState(DEFAULT_OUTPUT)
  const [keyPath, setKeyPath] = useState(DEFAULT_KEY)
  const [running, setRunning] = useState(false)
  const [error, setError] = useState('')
  const [result, setResult] = useState('')

  const handleBuild = async () => {
    setError('')
    setResult('')
    setRunning(true)
    try {
      // Blank inputs fall back to the engine-side defaults (undefined body).
      const out = outputPath.trim() || undefined
      const key = keyPath.trim() || undefined
      const reply = await client.buildCarc(out, key)
      if (reply.status !== 'ok') {
        setError(reply.error ?? 'Build failed')
        return
      }
      const parts = ['Built ' + (reply.path ?? out ?? DEFAULT_OUTPUT)]
      if (typeof reply.size === 'number') parts.push(`${reply.size} bytes`)
      if (typeof reply.files === 'number') parts.push(`${reply.files} files`)
      setResult(parts.join(' — '))
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    } finally {
      setRunning(false)
    }
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">Build</div>

      {/* CARC packaging — POST /api/build */}
      <section className="explorer-section">
        <div className="explorer-section-title">CARC PACKAGE</div>
        <input
          className="explorer-filter"
          placeholder={DEFAULT_OUTPUT}
          aria-label="CARC output path"
          value={outputPath}
          onChange={(e) => setOutputPath(e.target.value)}
        />
        <input
          className="explorer-filter"
          placeholder={DEFAULT_KEY}
          aria-label="CARC key path"
          value={keyPath}
          onChange={(e) => setKeyPath(e.target.value)}
        />
        <button
          className="primary"
          disabled={running}
          onClick={() => void handleBuild()}
        >
          {running ? 'Building…' : 'Build CARC'}
        </button>
        <div className="explorer-empty">
          Packs scripts/ + assets/ into an encrypted CARC archive. Paths stay
          confined under build/.
        </div>
      </section>

      {/* Web package — script-only: no HTTP endpoint exists for it. */}
      <section className="explorer-section">
        <div className="explorer-section-title">WEB PACKAGE (SCRIPT-ONLY)</div>
        <div className="explorer-empty">
          Web-site packaging is provided by the repository script
          scripts/package_game.sh — the engine exposes no HTTP endpoint for it,
          so this panel cannot run it for you.
        </div>
        <input
          className="explorer-filter"
          readOnly
          aria-label="Web package command"
          title="Run from the repo root (git bash)"
          value="bash scripts/package_game.sh demo/example_game"
          onFocus={(e) => e.currentTarget.select()}
        />
        <div className="explorer-empty">
          Produces dist/&lt;game&gt;/ — a self-contained static site for any
          static host.
        </div>
      </section>

      {/* Run — already provided by the Debug panel; not duplicated here. */}
      <section className="explorer-section">
        <div className="explorer-section-title">RUN</div>
        <div className="explorer-empty">
          Run the current scene from the Debug panel (&quot;Run Current
          Scene&quot;).
        </div>
        <button onClick={() => setSideView('debug')}>Open Debug Panel</button>
      </section>

      {error && <div className="panel-error">{error}</div>}
      {result && <div className="panel-msg">{result}</div>}
    </div>
  )
}
