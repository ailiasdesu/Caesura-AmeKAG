import { useState } from 'react'
import { RpcError, type EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

/** Defaults mirror the engine's POST /api/build fallbacks (EditorServer.cpp). */
const DEFAULT_OUTPUT = 'build/game.carc'
const DEFAULT_KEY = 'build/game.key'
/** Defaults mirror POST /api/package/web (EditorServer.cpp). */
const DEFAULT_STORY = 'demo/example_game/story.ks'
const DEFAULT_WEB_OUT = 'example_game'

/** Extract {error, logTail} from a failed RPC reply body, if any. */
function failureDetail(e: unknown): { error?: string; logTail?: string } {
  if (e instanceof RpcError && e.body && typeof e.body === 'object') {
    const body = e.body as { error?: unknown; logTail?: unknown }
    return {
      error: typeof body.error === 'string' ? body.error : undefined,
      logTail: typeof body.logTail === 'string' ? body.logTail : undefined,
    }
  }
  return {}
}

/**
 * Build Manager panel (Sprint 5, task book §Phase1).
 *
 * CARC section drives POST /api/build via EngineClient.buildCarc().
 * The Web Package section drives POST /api/package/web via
 * EngineClient.packageWeb(): the engine validates a whitelisted story path
 * (assets/ demo/ tests/projects/ projects/), sanitizes the output name to
 * [A-Za-z0-9_-] and runs scripts/package_game.sh into dist/<name> (confined
 * server-side). The panel surfaces the engine's log tail so packaging
 * failures stay diagnosable without leaving the IDE.
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

  const [webStoryPath, setWebStoryPath] = useState(DEFAULT_STORY)
  const [webOutName, setWebOutName] = useState(DEFAULT_WEB_OUT)
  const [webRunning, setWebRunning] = useState(false)
  const [webError, setWebError] = useState('')
  const [webSummary, setWebSummary] = useState('')
  const [webLogTail, setWebLogTail] = useState('')

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

  const handlePackageWeb = async () => {
    setWebError('')
    setWebSummary('')
    setWebLogTail('')
    setWebRunning(true)
    try {
      // Blank inputs fall back to the engine-side defaults.
      const story = webStoryPath.trim() || undefined
      const name = webOutName.trim() || undefined
      const reply = await client.packageWeb(story, name)
      if (!reply.ok) {
        setWebError(reply.error ?? 'Packaging failed')
        setWebLogTail(reply.logTail ?? '')
        return
      }
      setWebSummary(
        `Packaged ${reply.outputDir ?? DEFAULT_WEB_OUT} — a static web site ready for any host`,
      )
      setWebLogTail(reply.logTail ?? '')
    } catch (e) {
      // Script failures arrive as HTTP 500 whose body carries the engine
      // error text plus the script log tail: keep both visible so the user
      // can see which packaging step failed.
      const detail = failureDetail(e)
      setWebError(detail.error ?? (e instanceof Error ? e.message : String(e)))
      setWebLogTail(detail.logTail ?? '')
    } finally {
      setWebRunning(false)
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

      {/* Web package — POST /api/package/web runs the repo script engine-side. */}
      <section className="explorer-section">
        <div className="explorer-section-title">WEB PACKAGE</div>
        <input
          className="explorer-filter"
          placeholder={DEFAULT_STORY}
          aria-label="Web package story path"
          value={webStoryPath}
          onChange={(e) => setWebStoryPath(e.target.value)}
        />
        <input
          className="explorer-filter"
          placeholder={DEFAULT_WEB_OUT}
          aria-label="Web package output name"
          value={webOutName}
          onChange={(e) => setWebOutName(e.target.value)}
        />
        <button
          className="primary"
          disabled={webRunning}
          onClick={() => void handlePackageWeb()}
        >
          {webRunning ? 'Packaging…' : 'Package Web'}
        </button>
        <div className="explorer-empty">
          Executed by the engine endpoint POST /api/package/web, which runs
          scripts/package_game.sh for you: story paths are whitelisted
          (assets/ demo/ tests/projects/ projects/) and output is confined to
          dist/&lt;name&gt;/ — a self-contained static site for any static
          host.
        </div>
        {webError && <div className="panel-error">{webError}</div>}
        {webSummary && <div className="panel-msg">{webSummary}</div>}
        {webLogTail && (
          <pre className="explorer-empty" style={{ whiteSpace: 'pre-wrap' }}>
            {webLogTail}
          </pre>
        )}
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
