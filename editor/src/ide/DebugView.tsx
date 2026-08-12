import { useCallback, useEffect, useState } from 'react'
import type { DebugStateReply, EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

export function DebugView({ client }: Props) {
  const [state, setState] = useState<DebugStateReply | null>(null)
  const [scene, setScene] = useState('assets/script/main.ks')
  const [line, setLine] = useState('1')
  const [msg, setMsg] = useState('')
  const [script, setScript] = useState('')
  const [running, setRunning] = useState(false)
  const setEngine = useEditor((s) => s.setEngine)

  const refresh = useCallback(async () => {
    try {
      const s = await client.debugState()
      setState(s)
      setEngine({
        engineConnected: true,
        engineScene: s.scene ?? '',
        engineToken: typeof s.token_index === 'number' ? s.token_index : 0,
        enginePaused: s.paused === true,
      })
    } catch {
      setEngine({ engineConnected: false })
    }
  }, [client, setEngine])

  useEffect(() => {
    void refresh()
    const t = setInterval(() => void refresh(), 1500)
    return () => clearInterval(t)
  }, [refresh])

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
    try {
      await client.setBreakpoint(scene, Number(line) || 1)
      setMsg(`Breakpoint: ${scene}:${line}`)
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
    }
  }

  const clearBp = async () => {
    try {
      await client.clearBreakpoints()
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

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Run and Debug
        <span className="spacer" />
        <button onClick={() => void refresh()}>↻</button>
      </div>

      <div className="debug-state">
        <span className="debug-label">scene</span>
        <span className="debug-value">{state?.scene || '—'}</span>
        <span className="debug-label">token</span>
        <span className="debug-value">{state?.token_index ?? '—'}</span>
        <span className={`debug-badge ${state?.paused ? 'paused' : 'running'}`}>
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
        <button className="primary" onClick={() => void run()} disabled={running}>
          {running ? 'Running…' : 'Run'}
        </button>
        <button onClick={() => void stop()}>Stop</button>
        <button onClick={() => void cont()}>Continue</button>
      </div>

      <div className="panel-subtitle">KAG BREAKPOINTS</div>
      <div className="bp-row">
        <input
          value={scene}
          onChange={(e) => setScene(e.target.value)}
          placeholder="assets/script/main.ks"
          title="Scene path"
        />
        <input
          className="bp-line"
          value={line}
          onChange={(e) => setLine(e.target.value)}
          placeholder="line"
          title="Token line"
        />
      </div>
      <div className="bp-actions">
        <button onClick={() => void setBp()}>Set Breakpoint</button>
        <button onClick={() => void clearBp()}>Clear All</button>
      </div>

      {msg && <div className="panel-msg">{msg}</div>}
    </div>
  )
}
