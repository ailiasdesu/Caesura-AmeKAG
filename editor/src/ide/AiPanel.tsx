import { useState } from 'react'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

interface AiReply {
  text: string
  error: string
}

/** Battle 4c: AI-assisted scene writing panel.
 *  Calls the engine's aiwriter via /api/eval (local LLM, Ollama default),
 *  then inserts the generated KAG tags into the active document. */
export function AiPanel({ client }: Props) {
  const [speakers, setSpeakers] = useState('Aoi, Ryo')
  const [topic, setTopic] = useState('a quiet evening')
  const [lines, setLines] = useState('5')
  const [busy, setBusy] = useState(false)
  const [msg, setMsg] = useState('')
  const insertIntoActive = useEditor((s) => s.insertIntoActive)
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)

  const active = docs.find((d) => d.path === activePath) ?? null

  const call = async (method: string, args: unknown[]): Promise<AiReply | null> => {
    setBusy(true)
    setMsg('')
    try {
      const argStr = args
        .map((a) => (typeof a === 'string' ? `[=[${a}]=]` : JSON.stringify(a)))
        .join(', ')
      const code =
        `local aw = require('kag.aiwriter'); ` +
        `return aw.json('${method}'` +
        (args.length > 0 ? ', ' + argStr : '') +
        `)`
      const json = await client.evalRaw(code)
      const parsed = JSON.parse(json) as AiReply[]
      return parsed[0] ?? null
    } catch (e) {
      setMsg(e instanceof Error ? e.message : String(e))
      return null
    } finally {
      setBusy(false)
    }
  }

  const generate = async () => {
    const r = await call('generate_dialogue', [
      { speakers, topic, lines: Number(lines) || 5 },
    ])
    if (!r) return
    if (r.error && !r.text) {
      setMsg(`AI error: ${r.error} (is Ollama running on :11434?)`)
      return
    }
    if (active) {
      insertIntoActive('\n' + r.text + '\n')
      setMsg(`Inserted ${r.text.split('\n').length} lines`)
    } else {
      setMsg('Open a script first — generated:\n' + r.text)
    }
  }

  const continueScene = async () => {
    if (!active) {
      setMsg('Open a .ks script to continue it')
      return
    }
    const r = await call('continue_scene', [active.content.slice(-1500), { lines: Number(lines) || 5 }])
    if (!r) return
    if (r.error && !r.text) {
      setMsg(`AI error: ${r.error}`)
      return
    }
    insertIntoActive('\n' + r.text + '\n')
    setMsg(`Appended ${r.text.split('\n').length} lines`)
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        AI Writer
        <span className="spacer" />
        <span className="scene-counts">local LLM</span>
      </div>
      <div className="ai-form">
        <label className="ai-label">
          Speakers
          <input
            value={speakers}
            onChange={(e) => setSpeakers(e.target.value)}
            placeholder="Aoi, Ryo"
          />
        </label>
        <label className="ai-label">
          Topic
          <input
            value={topic}
            onChange={(e) => setTopic(e.target.value)}
            placeholder="a quiet evening"
          />
        </label>
        <label className="ai-label ai-lines">
          Lines
          <input
            className="ai-lines-input"
            value={lines}
            onChange={(e) => setLines(e.target.value)}
          />
        </label>
        <div className="ai-actions">
          <button className="primary" onClick={() => void generate()} disabled={busy}>
            {busy ? 'Generating…' : 'Generate Dialogue'}
          </button>
          <button onClick={() => void continueScene()} disabled={busy || !active}>
            Continue Scene
          </button>
        </div>
        {msg && <div className="panel-msg ai-msg">{msg}</div>}
        <p className="visual-hint">
          Uses the engine's local LLM (Ollama :11434 by default). Generated
          KAG tags are inserted at the end of the active script — sanitized so
          prose becomes comments.
        </p>
      </div>
    </div>
  )
}
