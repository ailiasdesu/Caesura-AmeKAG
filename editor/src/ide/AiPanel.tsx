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

/** Lua long-bracket string literal safe for arbitrary content: picks an
 *  equals-run longer than any `]=` sequence inside the string, so `]=]`
 *  in user/mod content cannot break out into arbitrary Lua via /api/eval
 *  (review should-fix). */
function luaString(s: string): string {
  const body = String(s)
  let maxRun = 0
  const re = /\]={1,}(?=\[)/g
  let m: RegExpExecArray | null
  while ((m = re.exec(body))) {
    maxRun = Math.max(maxRun, m[0].length - 1)
  }
  const eq = '='.repeat(maxRun + 1)
  return `[${eq}[${body}]${eq}]`
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
        .map((a) => (typeof a === 'string' ? luaString(a) : JSON.stringify(a)))
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

  // ------------------------------------------------------------------
  // Dev Assist (Battle 4c extension): the LLM helps the DEVELOPER —
  // explain diagnostics, review the active scene, generate scene
  // skeletons. Same /api/eval bridge, kag/aidev module.
  // ------------------------------------------------------------------
  const [spec, setSpec] = useState('')
  const [diagMsg, setDiagMsg] = useState('')
  const [devMsg, setDevMsg] = useState('')

  const devCall = async (method: string, args: unknown[]): Promise<AiReply | null> => {
    setBusy(true)
    try {
      const argStr = args
        .map((a) => (typeof a === 'string' ? luaString(a) : JSON.stringify(a)))
        .join(', ')
      const code =
        `local ad = require('kag.aidev'); ` +
        `return ad.json('${method}'` +
        (args.length > 0 ? ', ' + argStr : '') +
        `)`
      const json = await client.evalRaw(code)
      const parsed = JSON.parse(json) as AiReply[]
      return parsed[0] ?? null
    } catch (e) {
      setDevMsg(e instanceof Error ? e.message : String(e))
      return null
    } finally {
      setBusy(false)
    }
  }

  const explainDiag = async () => {
    const line = diagMsg.match(/^(\d+)\s*[:：]/)
    const diag = {
      scene: active?.path ?? '',
      line: line ? Number(line[1]) : 0,
      message: diagMsg,
    }
    const r = await devCall('explain_diagnostic', [diag, { llm: true }])
    if (!r) return
    setDevMsg(r.error && !r.text ? `AI error: ${r.error}` : r.text)
  }

  const reviewScene = async () => {
    if (!active) {
      setDevMsg('Open a .ks script to review')
      return
    }
    const r = await devCall('review_scene', [active.content.slice(-4000)])
    if (!r) return
    if (r.error && !r.text) {
      setDevMsg(`AI error: ${r.error}`)
      return
    }
    try {
      const findings = JSON.parse(r.text) as { line: number; message: string }[]
      if (findings.length === 0) {
        setDevMsg('✅ 结构检查通过（无未闭合块、无缺失 [end]）')
      } else {
        setDevMsg(
          findings.map((f) => `L${f.line}: ${f.message}`).join('\n')
        )
      }
    } catch {
      setDevMsg(r.text)
    }
  }

  const genScene = async () => {
    if (!spec.trim()) {
      setDevMsg('Describe the scene first (e.g. "a rainy classroom confession")')
      return
    }
    const r = await devCall('gen_scene', [spec])
    if (!r) return
    if (r.error && !r.text) {
      setDevMsg(`AI error: ${r.error}`)
      return
    }
    if (active) {
      insertIntoActive('\n' + r.text + '\n')
      setDevMsg(`Inserted ${r.text.split('\n').length} lines (review warnings appended as comments)`)
    } else {
      setDevMsg('Open a script first — generated:\n' + r.text)
    }
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

      <div className="panel-title dev-title">
        Dev Assist
        <span className="spacer" />
        <span className="scene-counts">LLM + static</span>
      </div>
      <div className="ai-form">
        <label className="ai-label">
          Scene spec
          <input
            value={spec}
            onChange={(e) => setSpec(e.target.value)}
            placeholder="a rainy classroom confession with a choice"
          />
        </label>
        <div className="ai-actions">
          <button className="primary" onClick={() => void genScene()} disabled={busy}>
            Generate Scene
          </button>
        </div>
        <label className="ai-label">
          Diagnostic (paste ks_check/LSP message, optional "N: " prefix)
          <input
            value={diagMsg}
            onChange={(e) => setDiagMsg(e.target.value)}
            placeholder="3: unknown KAG command 'wiat'"
          />
        </label>
        <div className="ai-actions">
          <button onClick={() => void explainDiag()} disabled={busy || !diagMsg.trim()}>
            Explain Diagnostic
          </button>
          <button onClick={() => void reviewScene()} disabled={busy || !active}>
            Review Scene
          </button>
        </div>
        {devMsg && <div className="panel-msg ai-msg dev-msg">{devMsg}</div>}
        <p className="visual-hint">
          Dev Assist runs entirely in the engine: local rule-based explainer
          and structural review work offline; the LLM enriches explanations
          and generates skeletons (sanitized + self-reviewed).
        </p>
      </div>
    </div>
  )
}
