import { useCallback, useEffect, useState } from 'react'
import type { BreakpointSpec, DebugStateReply, EngineClient, FrameReply } from '../lib/rpc'
import { buildRunSceneSnippet, scenePathForDoc } from '../lib/sceneRun'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

/** Format an inspect result value with a small type tag for display. */
function formatInspectValue(value: unknown): { type: string; text: string } {
  if (value === null || value === undefined) {
    return { type: 'nil', text: String(value ?? 'nil') }
  }
  if (typeof value === 'string') {
    return { type: 'string', text: value }
  }
  if (typeof value === 'number') {
    return { type: 'number', text: String(value) }
  }
  if (typeof value === 'boolean') {
    return { type: 'boolean', text: String(value) }
  }
  if (Array.isArray(value)) {
    return { type: 'table[' + value.length + ']', text: JSON.stringify(value) }
  }
  return { type: 'table', text: JSON.stringify(value) }
}

/** Stable dedup key for a breakpoint entry (scene:line). */
function bpKey(bp: BreakpointSpec): string {
  return bp.source + ':' + bp.line
}

export function DebugView({ client }: Props) {
  const [state, setState] = useState<DebugStateReply | null>(null)
  const [scene, setScene] = useState('assets/script/main.ks')
  const [line, setLine] = useState('1')
  const [msg, setMsg] = useState('')
  const [script, setScript] = useState('')
  const [running, setRunning] = useState(false)
  const [breakpoints, setBreakpoints] = useState<BreakpointSpec[]>([])
  const [inspectName, setInspectName] = useState('')
  const [inspectFrame, setInspectFrame] = useState('int')
  const [inspecting, setInspecting] = useState(false)
  const [inspectResult, setInspectResult] = useState<{ name: string; value: unknown } | null>(null)
  const [inspectError, setInspectError] = useState('')
  const [capturing, setCapturing] = useState(false)
  const [frame, setFrame] = useState<FrameReply | null>(null)
  const [frameError, setFrameError] = useState('')
  const engineConnected = useEditor((s) => s.engineConnected)
  const enginePaused = useEditor((s) => s.enginePaused)
  const setEngine = useEditor((s) => s.setEngine)
  const activePath = useEditor((s) => s.activePath)

  // The runnable scene for the currently open document (.ks only).
  const activeScene = scenePathForDoc(activePath)

  const refresh = useCallback(async () => {
    try {
      const s = await client.debugState()
      setState(s)
      setEngine({
        engineConnected: true,
        engineScene: s.scene ?? '',
        engineToken: typeof s.token_index === 'number' ? s.token_index : 0,
        enginePaused: s.paused === true,
        engineCmd: s.current_cmd ?? '',
      })
    } catch {
      setEngine({ engineConnected: false, enginePaused: false })
    }
  }, [client, setEngine])

  useEffect(() => {
    void refresh()
    const t = setInterval(() => void refresh(), 1500)
    return () => clearInterval(t)
  }, [refresh])

  // When the active doc is a .ks scene, point the breakpoint scene box at it
  // so breakpoints default to the scene being edited/run.
  useEffect(() => {
    const scene = scenePathForDoc(activePath)
    if (scene) setScene(scene)
  }, [activePath])

  const run = async () => {
    setRunning(true)
    setMsg('')
    try {
      await client.run(script)
      setMsg('Script completed')
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    } finally {
      setRunning(false)
    }
  }

  /** Run the currently open .ks scene: stop any running scene, then start
   *  the new one via kag_runner through the /api/eval channel. */
  const runCurrentScene = async () => {
    const scene = scenePathForDoc(activePath)
    if (!scene) {
      setMsg('Open a .ks scene to run it')
      return
    }
    setRunning(true)
    setMsg('')
    try {
      const result = await client.evalRaw(buildRunSceneSnippet(scene))
      setMsg('Scene: ' + scene + (result.trim() ? ' → ' + result.trim() : ''))
      await refresh()
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    } finally {
      setRunning(false)
    }
  }

  /** Hot-reload the engine's current scene (client.reload) and refresh state. */
  const reloadScene = async () => {
    setMsg('')
    try {
      await client.reload()
      setMsg('Scene reloaded')
      await refresh()
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const stop = async () => {
    try {
      await client.stop()
      setMsg('Stop requested')
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const setBp = async () => {
    setMsg('')
    const source = scene.trim() || 'assets/script/main.ks'
    const target = Number(line) || 1
    try {
      await client.setBreakpoint(source, target)
      // Add to the local list (dedupe by scene:line, keep line-sorted).
      setBreakpoints((prev) => {
        const next = prev.filter((b) => bpKey(b) !== source + ':' + target)
        next.push({ source, line: target })
        next.sort((a, b) =>
          a.source === b.source ? a.line - b.line : a.source.localeCompare(b.source),
        )
        return next
      })
      setMsg('Breakpoint: ' + source + ':' + target)
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const removeBp = async (bp: BreakpointSpec) => {
    try {
      await client.removeBreakpoint(bp.source, bp.line)
      setBreakpoints((prev) => prev.filter((b) => bpKey(b) !== bpKey(bp)))
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const clearBp = async () => {
    try {
      await client.clearBreakpoints()
      setBreakpoints([])
      setMsg('Breakpoints cleared')
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const cont = async () => {
    try {
      await client.debugContinue()
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const doInspect = async () => {
    const name = inspectName.trim()
    if (!name) {
      setInspectError('Enter a variable name to inspect')
      setInspectResult(null)
      return
    }
    setInspecting(true)
    setInspectError('')
    setInspectResult(null)
    const frameNo = inspectFrame.trim() === '' ? 0 : Number(inspectFrame)
    try {
      const value = await client.inspect(name, Number.isFinite(frameNo) ? frameNo : 0, false)
      setInspectResult({ name, value })
    } catch (e) {
      setInspectError(e instanceof Error ? e.message : String(e))
    } finally {
      setInspecting(false)
    }
  }

  const doCapture = async () => {
    setCapturing(true)
    setFrameError('')
    setFrame(null)
    try {
      const f = await client.frame(640, 360)
      setFrame(f)
      if (f.error) setFrameError(f.error)
    } catch (e) {
      setFrame(null)
      setFrameError(e instanceof Error ? e.message : String(e))
    } finally {
      setCapturing(false)
    }
  }

  const offline = !engineConnected
  const formatted = inspectResult ? formatInspectValue(inspectResult.value) : null

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Run and Debug
        <span className="spacer" />
        <button onClick={() => void refresh()}>↻</button>
      </div>

      {offline && (
        <div className="panel-error">Engine disconnected — debug controls disabled</div>
      )}

      <div className="debug-state">
        <span className="debug-label">scene</span>
        <span className="debug-value">{state?.scene || '—'}</span>
        <span className="debug-label">token</span>
        <span className="debug-value">{state?.token_index ?? '—'}</span>
        <span className={'debug-badge ' + (state?.paused ? 'paused' : 'running')}>
          {state?.paused ? 'paused' : 'running'}
        </span>
      </div>

      <div className="panel-subtitle">RUN SCRIPT</div>
      <textarea
        className="debug-script"
        placeholder="-- Lua to run in the engine (optional)"
        value={script}
        onChange={(e) => setScript(e.target.value)}
        spellCheck={false}
      />
      <div className="bp-actions">
        <button
          className="primary"
          onClick={() => void runCurrentScene()}
          disabled={running || offline || !activeScene}
          title={activeScene ? 'Run ' + activeScene : 'Open a .ks scene to run it'}
        >
          {running ? 'Running…' : 'Run Current Scene'}
        </button>
        <button onClick={() => void reloadScene()} disabled={offline} title="Hot-reload the engine scene">
          Reload Scene
        </button>
      </div>
      {!activeScene && (
        <div className="panel-msg">
          Run Current Scene needs an open .ks scene (current doc: {activePath || 'none'})
        </div>
      )}
      <div className="bp-actions">
        <button onClick={() => void run()} disabled={running || offline}>
          Run Raw
        </button>
        <button onClick={() => void stop()} disabled={offline}>
          Stop
        </button>
        <button onClick={() => void cont()} disabled={offline || !enginePaused} title="Resume when paused">
          Continue
        </button>
      </div>

      <div className="panel-subtitle">KAG BREAKPOINTS</div>
      <div className="bp-row">
        <input
          value={scene}
          onChange={(e) => setScene(e.target.value)}
          placeholder="assets/script/main.ks"
          title="Scene path"
          disabled={offline}
        />
        <input
          className="bp-line"
          value={line}
          onChange={(e) => setLine(e.target.value)}
          placeholder="line"
          title="Token line"
          disabled={offline}
        />
      </div>
      <div className="bp-actions">
        <button onClick={() => void setBp()} disabled={offline}>Set Breakpoint</button>
        <button onClick={() => void clearBp()} disabled={offline || breakpoints.length === 0}>
          Clear All
        </button>
      </div>

      {breakpoints.length > 0 && (
        <ul className="bp-list">
          {breakpoints.map((bp) => (
            <li className="bp-item" key={bpKey(bp)}>
              <span className="bp-item-src" title={bp.source}>
                {bp.source}
              </span>
              <span className="bp-item-line">:{bp.line}</span>
              <button
                className="bp-item-rm"
                onClick={() => void removeBp(bp)}
                disabled={offline}
                aria-label={'Remove ' + bp.source + ':' + bp.line}
              >
                ✕
              </button>
            </li>
          ))}
        </ul>
      )}

      <div className="panel-subtitle">INSPECT</div>
      <div className="bp-row">
        <input
          value={inspectName}
          onChange={(e) => setInspectName(e.target.value)}
          placeholder="variable name"
          title="Variable to inspect (engine scope)"
          disabled={offline}
        />
        <input
          className="bp-line"
          value={inspectFrame}
          onChange={(e) => setInspectFrame(e.target.value)}
          placeholder="frame"
          title="Call-stack frame (default 0)"
          disabled={offline}
        />
      </div>
      <div className="bp-actions">
        <button onClick={() => void doInspect()} disabled={offline || inspecting}>
          {inspecting ? 'Inspecting…' : 'Inspect'}
        </button>
      </div>
      {inspectError && <div className="panel-error">{inspectError}</div>}
      {formatted && (
        <div className="inspect-result">
          <div className="inspect-result-head">
            <span className="inspect-name">{inspectResult?.name}</span>
            <span className="inspect-type">{formatted.type}</span>
          </div>
          <pre className="inspect-value" title={formatted.text}>
            {formatted.text || '(empty)'}
          </pre>
        </div>
      )}

      <div className="panel-subtitle">FRAME CAPTURE</div>
      <div className="bp-actions">
        <button onClick={() => void doCapture()} disabled={offline || capturing}>
          {capturing ? 'Capturing…' : 'Capture Frame'}
        </button>
      </div>
      {frameError && <div className="panel-error">{frameError}</div>}
      {frame ? (
        frame.png ? (
          <div className="frame-capture">
            <img className="frame-img" src={'data:image/png;base64,' + frame.png} alt="debug frame" />
            {(typeof frame.width === 'number' || typeof frame.height === 'number') && (
              <div className="frame-meta">
                {frame.width}×{frame.height}
              </div>
            )}
          </div>
        ) : (
          <div className="explorer-empty">Frame captured ({frame.width ?? '?'}×{frame.height ?? '?'}, no pixels)</div>
        )
      ) : (
        !capturing && <div className="explorer-empty">No frame captured yet</div>
      )}

      {msg && <div className="panel-msg">{msg}</div>}
    </div>
  )
}