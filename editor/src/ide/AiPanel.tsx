import { useEffect, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import type { EngineClient } from '../lib/rpc'
import { luaString, luaValue } from '../lib/luaString'
import { useEditor } from '../store'
import {
  AI_CHAT_KEY,
  clearChat,
  loadChat,
  saveChat,
  nextMessageId,
  type ChatMessage,
} from '../lib/chatHistory'
import { splitCodeBlocks, type CodeBlock } from '../lib/codeBlocks'
import { withTimeout } from '../lib/promiseUtil'

interface Props {
  client: EngineClient
}

interface AiReply {
  text: string
  error: string
}

/** How long an engine /api/eval query may run before we degrade the UI. */
const QUERY_TIMEOUT_MS = 20000

/** Battle 4c: AI-assisted scene writing panel.
 *  Calls the engine's aiwriter via /api/eval (local LLM, Ollama default),
 *  then inserts the generated KAG tags into the active document. The "Ask"
 *  section adds a free-form LLM chat with context injection, code-block
 *  rendering, a persisted transcript, and engine-state gating. */

function renderBlocks(blocks: CodeBlock[], keyBase: string): ReactNode {
  return blocks.map((b, i) =>
    b.kind === 'code' ? (
      <pre key={keyBase + '-c' + i} className="ai-code" data-testid="ai-code-block">
        <code>{b.content}</code>
      </pre>
    ) : (
      <span key={keyBase + '-t' + i}>{b.content}</span>
    ),
  );
}

export function AiPanel({ client }: Props) {
  // ---- shared engine/state selectors ----
  const insertIntoActive = useEditor((s) => s.insertIntoActive)
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const engineConnected = useEditor((s) => s.engineConnected)
  const enginePaused = useEditor((s) => s.enginePaused)
  const editorSelection = useEditor((s) => s.editorSelection)
  const [busy, setBusy] = useState(false)
  const [msg, setMsg] = useState('')

  const active = docs.find((d) => d.path === activePath) ?? null
  const selectionText =
    editorSelection && editorSelection.path === activePath ? editorSelection.text : null

  // ---- AI Writer (Battle 4c) ----
  const [speakers, setSpeakers] = useState('Aoi, Ryo')
  const [topic, setTopic] = useState('a quiet evening')
  const [lines, setLines] = useState('5')

  const call = async (method: string, args: unknown[]): Promise<AiReply | null> => {
    setBusy(true)
    setMsg('')
    try {
      const argStr = args
        .map((a) => (typeof a === 'string' ? luaString(a) : luaValue(a)))
        .join(', ')
      const code =
        "local aw = require('kag.aiwriter'); " +
        "return aw.json('" + method + "'" +
        (args.length > 0 ? ', ' + argStr : '') +
        ")"
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
    const r = await call('continue_scene', [
      active.content.slice(-1500),
      { lines: Number(lines) || 5 },
    ])
    if (!r) return
    if (r.error && !r.text) {
      setMsg(`AI error: ${r.error}`)
      return
    }
    insertIntoActive('\n' + r.text + '\n')
    setMsg(`Appended ${r.text.split('\n').length} lines`)
  }

  // ---- Dev Assist (Battle 4c extension) ----
  const [spec, setSpec] = useState('')
  const [diagMsg, setDiagMsg] = useState('')
  const [devMsg, setDevMsg] = useState('')

  const devCall = async (method: string, args: unknown[]): Promise<AiReply | null> => {
    setBusy(true)
    try {
      const argStr = args
        .map((a) => (typeof a === 'string' ? luaString(a) : luaValue(a)))
        .join(', ')
      const code =
        "local ad = require('kag.aidev'); " +
        "return ad.json('" + method + "'" +
        (args.length > 0 ? ', ' + argStr : '') +
        ")"
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
        setDevMsg(findings.map((f) => `L${f.line}: ${f.message}`).join('\n'))
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
      setDevMsg(
        `Inserted ${r.text.split('\n').length} lines (review warnings appended as comments)`,
      )
    } else {
      setDevMsg('Open a script first — generated:\n' + r.text)
    }
  }

  // ---- Ask — free-form LLM chat with context injection (Battle 4c+) ----
  const [prompt, setPrompt] = useState('')
  const [includeDoc, setIncludeDoc] = useState(false)
  const [useSelection, setUseSelection] = useState(false)
  const [chat, setChat] = useState<ChatMessage[]>(() => loadChat(AI_CHAT_KEY))
  const [askError, setAskError] = useState('')
  const transcriptRef = useRef(chat)
  const scrollRef = useRef<HTMLDivElement | null>(null)

  // item 3: restore the persisted transcript when the panel mounts; keep
  // the transcript mirrored to localStorage on every change so a re-open
  // restores it (also covered by the lazy initializer above).
  useEffect(() => {
    setChat(loadChat(AI_CHAT_KEY))
  }, [])

  useEffect(() => {
    transcriptRef.current = chat
    saveChat(chat, AI_CHAT_KEY)
  }, [chat])

  const pushMessage = (m: ChatMessage) => {
    setChat((c) => {
      const next = [...c, m]
      return next.slice(-100)
    })
  }

  useEffect(() => {
    if (typeof scrollRef.current?.scrollTo === 'function') {
      scrollRef.current.scrollTo({ top: scrollRef.current.scrollHeight })
    }
  }, [chat])

  // item 2: encapsulate document / selection injection into the query payload.
  const buildContext = (): string => {
    const parts: string[] = []
    if (includeDoc && active) {
      parts.push('ACTIVE DOCUMENT (' + active.path + '):\n' + active.content.slice(-2000))
    }
    if (useSelection && selectionText) {
      parts.push('SELECTION:\n' + selectionText)
    }
    if (parts.length === 0) {
      return 'No document context requested. Answer generally about KAG script authoring.'
    }
    return parts.join('\n\n---\n\n')
  }

  const ask = async () => {
    setAskError('')
    const text = prompt.trim()
    if (!text) return
    if (!engineConnected) {
      setAskError('Engine disconnected — AI queries are unavailable.')
      return
    }
    setPrompt('')
    const sysContext = buildContext()
    const userMsg: ChatMessage = {
      id: nextMessageId(),
      role: 'user',
      text,
      time: Date.now(),
    }
    pushMessage(userMsg)
    // Full-response display: a single /api/eval round-trip returns the reply.
    // A short timeout guards against a hung engine (UI degrades to an error row).
    const payload = {
      system: sysContext,
      user: text,
      include_doc: includeDoc,
      use_selection: useSelection && !!selectionText,
      doc: active?.path ?? '',
      prev: transcriptRef.current.slice(-10, -1), // rolling context
    }
    const argStr = luaValue(payload)
    const code = "local ad = require('kag.aidev'); return ad.json('ask', " + argStr + ')'
    try {
      const json = await withTimeout(
        client.evalRaw(code),
        QUERY_TIMEOUT_MS,
        'AI query timed out after ' + QUERY_TIMEOUT_MS / 1000 + 's',
      )
      const parsed = JSON.parse(json) as AiReply[]
      const r = parsed[0] ?? null
      if (!r) {
        throw new Error('empty engine reply')
      }
      if (r.error && !r.text) {
        const errMsg: ChatMessage = {
          id: nextMessageId(),
          role: 'error',
          text: 'AI error: ' + r.error,
          time: Date.now(),
        }
        pushMessage(errMsg)
        return
      }
      const reply: ChatMessage = {
        id: nextMessageId(),
        role: 'assistant',
        text: r.text,
        time: Date.now(),
      }
      pushMessage(reply)
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e)
      setAskError(msg)
      pushMessage({ id: nextMessageId(), role: 'error', text: msg, time: Date.now() })
    }
  }

  const clearHistory = () => {
    clearChat(AI_CHAT_KEY)
    setChat([])
    setAskError('')
  }

  // item 4: engine-state gating.
  const disconnected = !engineConnected
  const hint =
    askError ||
    (disconnected
      ? 'Engine disconnected — open a connection to query the local LLM.'
      : enginePaused
        ? 'Engine paused — the game loop is stopped; AI queries still run.'
        : '')

  // ---- render ----
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
          <button
            className="primary"
            onClick={() => void generate()}
            disabled={busy || disconnected}
          >
            {busy ? 'Generating…' : 'Generate Dialogue'}
          </button>
          <button
            onClick={() => void continueScene()}
            disabled={busy || !active || disconnected}
          >
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
          <button
            className="primary"
            onClick={() => void genScene()}
            disabled={busy || disconnected}
          >
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
          <button
            onClick={() => void explainDiag()}
            disabled={busy || !diagMsg.trim() || disconnected}
          >
            Explain Diagnostic
          </button>
          <button
            onClick={() => void reviewScene()}
            disabled={busy || !active || disconnected}
          >
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

      <div className="panel-title dev-title">
        Ask
        <span className="spacer" />
        <span className="scene-counts">chat · code blocks · context</span>
      </div>
      {disconnected && (
        <div className="panel-msg ai-msg ai-warn" data-testid="ai-disconnected">
          Engine disconnected — AI panel disabled.
        </div>
      )}
      {!disconnected && enginePaused && (
        <div className="panel-msg ai-msg ai-note" data-testid="ai-paused">
          Engine paused — game loop stopped; AI queries still run.
        </div>
      )}
      <div className="ai-form">
        <div className="ai-context-row" role="group" aria-label="Query context">
          <label className="ai-check">
            <input
              type="checkbox"
              checked={includeDoc}
              onChange={(e) => setIncludeDoc(e.target.checked)}
              disabled={!active}
              data-testid="include-doc"
            />
            doc tail
          </label>
          <label className="ai-check">
            <input
              type="checkbox"
              checked={useSelection}
              onChange={(e) => setUseSelection(e.target.checked)}
              disabled={!selectionText}
              data-testid="include-selection"
            />
            selection
          </label>
          {!active && (
            <span className="ai-context-hint" data-testid="no-doc-hint">
              no doc open — open a .ks file to inject its tail
            </span>
          )}
        </div>
        <textarea
          className="ai-prompt"
          value={prompt}
          onChange={(e) => setPrompt(e.target.value)}
          placeholder="Ask the LLM — e.g. 'write a tense reveal with a choice'"
          rows={3}
          aria-label="AI prompt"
        />
        <div className="ai-actions">
          <button
            className="primary"
            onClick={() => void ask()}
            disabled={busy || disconnected || !prompt.trim()}
          >
            {busy ? 'Thinking…' : 'Ask'}
          </button>
          <button
            onClick={clearHistory}
            disabled={chat.length === 0}
            title="Clear conversation"
          >
            Clear
          </button>
        </div>
        {hint && (
          <div className="panel-msg ai-msg" data-testid="ask-hint">
            {hint}
          </div>
        )}
        <div className="ai-transcript" ref={scrollRef} data-testid="ai-transcript">
          {chat.length === 0 && (
            <div className="ai-empty" data-testid="ai-empty">
              No conversation yet — ask a question, the reply renders here and
              persists across panel re-opens.
            </div>
          )}
          {chat.map((m) => (
            <div
              key={m.id}
              className={'ai-msg-row ai-msg-row-' + m.role}
              data-role={m.role}
            >
              <div className="ai-msg-from">{m.role}</div>
              <div className="ai-msg-body">
                {m.role === 'user' ? (
                  m.text
                ) : m.role === 'assistant' ? (
                  renderBlocks(splitCodeBlocks(m.text), m.id)
                ) : (
                  <span className="ai-msg-error">{m.text}</span>
                )}
              </div>
            </div>
          ))}
        </div>
        <p className="visual-hint">
          Ask is a free-form chat that runs in the engine via kag.aidev.
          Context is injected only when you opt in; replies with
          <code>```code blocks```</code> are rendered as monospaced panels.
        </p>
      </div>
    </div>
  )
}
