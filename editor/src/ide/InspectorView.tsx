// G4: scene-element inspector. Reads the currently inspected element
// (path + line, set by SceneTree clicks) and renders its full parameter
// table — the detail line only shows the first storage/file/text/name
// param, while the inspector shows every key/value pair.
//
// Round 82 enhancements:
//  1. Engine runtime status strip — when the engine is connected and its
//     running scene matches this document (engineScene/engineToken written
//     into the store by useEnginePosition polling), show the live scene,
//     token index and paused state at the top of the panel.
//  2. Command-lint hints — for a selected [command] row, validate the command
//     name against KNOWN_COMMANDS and, for curated commands, hint which params
//     are unlisted (soft hint) vs. bare flags. Unknown command names are
//     surfaced as a warning.
//  3. Bidirectional label navigation — for a selected *label, jump the live
//     engine to that label (buildLabelJumpSnippet → /api/eval), and follow the
//     engine's current position back into the editor (token→source line). The
//     jump outcome ('ok' / 'missing' / 'no-ctx' / 'error') is surfaced as a
//     status line under the buttons, and a re-entry guard makes double-clicks
//     idempotent (no duplicate engine evals).
//
// The optional EngineClient is only used for the live-jump and follow-engine
// actions; with no client (or a disconnected engine) those degrade to a
// disabled state and the panel still renders fully.
import { useMemo, useRef, useState } from 'react'
import { useEditor } from '../store'
import { parseSceneElements } from './SceneTree'
import {
  buildLabelJumpSnippet,
  parseJumpResult,
  type LabelJumpStatus,
} from '../lib/engineJump'
import { parseSceneOutline, buildOutlineSections } from '../lib/sceneOutline'
import { sceneMatchesDoc, tokenToOutlineLine } from '../lib/enginePosition'
import { lintCommand, type CommandLint } from '../lib/commandLint'
import { revealEditorLine } from './EditorArea'
import type { EngineClient } from '../lib/rpc'

interface InspectorViewProps {
  /** Optional engine RPC client; used by the live label-jump / follow actions. */
  client?: EngineClient
}

/** Strip the surrounding brackets from a [command] tag text ("" when none). */
function commandWord(text: string): string {
  const m = text.trim().match(/^\[(\w+)\]?$/)
  return m ? m[1] : ''
}

/** The label name from a *label element text ("" when not a label). */
function labelName(text: string): string {
  const m = text.trim().match(/^\*([\w_]+)/)
  return m ? m[1] : ''
}

/** Basename of a scene path for the compact status strip. */
function sceneBasename(path: string): string {
  const parts = path.split('/')
  return parts[parts.length - 1] ?? path
}

/** Short hint text for a parameter value based on its lint verdict. */
function paramHint(lint: CommandLint | null, key: string, value: string): string {
  if (!lint) return value
  const verdict = lint.params[key]
  if (verdict === 'unlisted') return key + ' not listed for [' + lint.command + '] — possible typo'
  if (verdict === 'flag') return key + ' is a bare flag (no value)'
  return value
}

/**
 * Issue a label-jump eval against the live engine and classify the outcome so
 * the panel can surface feedback. Never throws: transport failures collapse
 * to 'error' so the affordance still renders instead of crashing the click.
 */
async function jumpEngineToLabel(
  client: EngineClient,
  label: string,
): Promise<LabelJumpStatus> {
  try {
    const result = await client.evalRaw(buildLabelJumpSnippet(label))
    return parseJumpResult(result)
  } catch {
    // engine rejection / transport failure — reported as an error state
    return 'error'
  }
}

/** Human-readable copy for the jump outcome line shown under the buttons. */
function jumpStatusText(status: LabelJumpStatus, label: string): string {
  switch (status) {
    case 'ok':
      return 'Jumped to *' + label
    case 'missing':
      return 'Label *' + label + ' not found in the running scene'
    case 'no-ctx':
      return 'No scene running in the engine'
    case 'error':
      return 'Engine jump failed — is the engine reachable?'
    default:
      return 'Engine replied: ' + status
  }
}

export function InspectorView({ client }: InspectorViewProps) {
  const docs = useEditor((s) => s.docs)
  const inspected = useEditor((s) => s.inspected)
  const engineConnected = useEditor((s) => s.engineConnected)
  const engineScene = useEditor((s) => s.engineScene)
  const engineToken = useEditor((s) => s.engineToken)
  const enginePaused = useEditor((s) => s.enginePaused)
  const requestReveal = useEditor((s) => s.requestReveal)
  const setInspected = useEditor((s) => s.setInspected)

  const doc = inspected ? (docs.find((d) => d.path === inspected.path) ?? null) : null
  const element = useMemo(() => {
    if (!doc || !inspected) return null
    return (
      parseSceneElements(doc.content).find((e) => e.line === inspected.line) ?? null
    )
  }, [doc, inspected])

  const sceneMatches = Boolean(
    doc && inspected && engineConnected && sceneMatchesDoc(engineScene, doc.path),
  )

  // Resolve the live engine token to a source line for the status strip and the
  // follow-engine action.
  const engineLine = useMemo(() => {
    if (!doc || !sceneMatches || typeof engineToken !== 'number' || engineToken < 1) {
      return null
    }
    const sections = buildOutlineSections(parseSceneOutline(doc.content))
    return tokenToOutlineLine(sections, engineToken)
  }, [doc, sceneMatches, engineToken])

  const isLabel = element?.type === 'label'
  const label = isLabel && element ? labelName(element.text) : ''

  // Live-jump feedback + re-entry guard. jumpStatus reports the engine's
  // reply ('ok' / 'missing' / 'no-ctx' / 'error') under the buttons. The pending
  // latch is a ref so it takes effect synchronously — a same-tick double-click
  // hits the guard before React re-renders, so no duplicate engine evals fire.
  const [jumpStatus, setJumpStatus] = useState<LabelJumpStatus | null>(null)
  const jumpPendingRef = useRef(false)

  const handleJumpEngineToLabel = () => {
    if (!isLabel || !label || !engineConnected || !client) return
    if (jumpPendingRef.current) return
    jumpPendingRef.current = true
    setJumpStatus(null)
    void jumpEngineToLabel(client, label).then((status) => {
      jumpPendingRef.current = false
      setJumpStatus(status)
    })
  }

  const handleFollowEngine = () => {
    if (!sceneMatches || !doc || engineLine === null) return
    requestReveal(doc.path, engineLine)
    revealEditorLine(doc.path, engineLine)
    setInspected(doc.path, engineLine)
  }

  if (!inspected) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Inspector</div>
        <div className="explorer-empty">Click a scene element to inspect it</div>
      </div>
    )
  }

  if (!doc || !element) {
    return (
      <div className="sidebar-pane">
        <div className="panel-title">Inspector</div>
        <div className="explorer-empty">Element not found (document closed or line removed)</div>
      </div>
    )
  }

  const params = Object.entries(element.params)
  const lineText = doc.content.split('\n')[element.line - 1] ?? ''

  const isCommand = element.type !== 'label' && element.text.startsWith('[')
  const command = isCommand ? commandWord(element.text) : ''
  const lint = isCommand ? lintCommand(command, element.params) : null

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Inspector
        <span className="spacer" />
        <span className="scene-counts">L{element.line}</span>
      </div>

      {sceneMatches && (
        <div className="inspector-engine">
          <span className="inspector-engine-label">engine</span>
          <span className="inspector-engine-value" title={engineScene}>
            {sceneBasename(engineScene)}
          </span>
          <span className="inspector-engine-value">
            #{typeof engineToken === 'number' ? engineToken : '?'}
          </span>
          <span className={'debug-badge ' + (enginePaused ? 'paused' : 'running')}>
            {enginePaused ? 'paused' : 'running'}
          </span>
        </div>
      )}

      <div className="inspector-body">
        <div className="state-row">
          <span>type</span>
          <b>{element.type}</b>
        </div>
        <div className="state-row">
          <span>command</span>
          <b>{element.text}</b>
        </div>

        {lint && !lint.knownCommand && (
          <div className="inspector-warn">
            Unknown command '{command}' — not in the recognized KAG command set.
          </div>
        )}

        {params.length === 0 ? (
          <div className="state-row">
            <span>params</span>
            <b>—</b>
          </div>
        ) : (
          params.map(([k, v]) => (
            <div className="state-row" key={k}>
              <span>{k}</span>
              <b
                className={
                  lint?.params[k] === 'unlisted'
                    ? 'inspector-param-unlisted'
                    : lint?.params[k] === 'flag'
                      ? 'inspector-param-flag'
                      : undefined
                }
                title={paramHint(lint, k, v)}
              >
                {v}
              </b>
            </div>
          ))
        )}

        {lint && lint.unlistedCount > 0 && (
          <div className="inspector-hint">
            {lint.unlistedCount} unlisted param{lint.unlistedCount > 1 ? 's' : ''} for [{command}] — verify spelling.
          </div>
        )}

        {isLabel && (
          <div className="inspector-jumps">
            <button
              className="primary"
              onClick={handleJumpEngineToLabel}
              disabled={!engineConnected || !client}
              title="Live-jump the running engine to this label"
            >
              Jump engine → label
            </button>
            <button
              onClick={handleFollowEngine}
              disabled={!sceneMatches || engineLine === null}
              title="Jump the editor to the engine's current position"
            >
              Follow engine position
            </button>
          </div>
        )}

        {isLabel && jumpStatus !== null && (
          <div
            className={
              'inspector-jump-status' + (jumpStatus === 'ok' ? ' ok' : ' warn')
            }
          >
            {jumpStatusText(jumpStatus, label)}
          </div>
        )}

        <div className="inspector-source" title={lineText}>
          {lineText || '(blank line)'}
        </div>
      </div>
    </div>
  )
}
