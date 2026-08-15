// G4 editor — scene outline panel (first increment).
//
// Read-only outline of a .ks scene: *labels as section headings, [command...]
// tags as command rows (command name + key params), and bare text lines as
// content. Presentational only — takes the scene text and an optional
// label-click callback; the caller (ExplorerView / a future store wiring)
// supplies the source and handles navigation.

import { useMemo } from 'react'
import { parseSceneOutline, buildOutlineSections } from '../lib/sceneOutline'
import type { OutlineItem } from './sceneOutlineTypes'

export interface SceneOutlineProps {
  /** Raw .ks scene text to outline. */
  source: string
  /** Invoked when a *label heading is clicked with the label name (no '*').
   *  Basic navigation affordance; no-op stub is fine for this increment. */
  onSelectLabel?: (label: string, line: number) => void
  /** Invoked when the label's per-section live-jump affordance is used
   *  (secondary click on the heading or the ▶ button). Drives the running
   *  engine scene to the label; optional — absent keeps the row read-only
   *  apart from the primary select handler. */
  onJumpToLabel?: (label: string, line: number) => void
  /** Live engine position: the source line the running scene is currently at
   *  (derived from token_index). When set, the matching section heading or
   *  item row is highlighted (outline-current). Disconnected / no scene → 0
   *  or null → no highlight. */
  currentLine?: number | null
  /** Live engine scene name (path) currently running; shown in the header
   *  next to the section count when the engine is connected. */
  currentScene?: string | null
}

/** Render one outline row: a command row or a text content line. */
function OutlineRow({ item, active }: { item: OutlineItem; active: boolean }) {
  const rowClass =
    'outline-row ' +
    (item.kind === 'command' ? 'outline-command' : 'outline-text') +
    (active ? ' outline-current' : '')
  if (item.kind === 'command') {
    const keyParams = Object.entries(item.params)
      .slice(0, 3)
      .map(([k, v]) => k + '=' + v)
      .join(' ')
    const cmdText = '[' + item.cmd + ']'
    return (
      <div className={rowClass} title={'line ' + item.line + ' — ' + cmdText}>
        <span className="outline-line">{item.line}</span>
        <span className="outline-cmd">{cmdText}</span>
        {keyParams && <span className="outline-cmd-params">{keyParams}</span>}
      </div>
    )
  }
  if (item.kind !== 'text') return null
  return (
    <div className={rowClass} title={'line ' + item.line}>
      <span className="outline-line">{item.line}</span>
      <span className="outline-content">{item.content}</span>
    </div>
  )
}

export function SceneOutline({
  source,
  onSelectLabel,
  onJumpToLabel,
  currentLine,
  currentScene,
}: SceneOutlineProps) {
  const sections = useMemo(
    () => buildOutlineSections(parseSceneOutline(source)),
    [source],
  )

  const handleLabel = (label: string, line: number) => {
    if (onSelectLabel) onSelectLabel(label, line)
  }

  const handleJump = (label: string, line: number) => {
    if (onJumpToLabel) onJumpToLabel(label, line)
  }

  // The row currently at the live engine position (a single source line maps
  // to at most one outline row — a section heading OR an item row).
  const current = typeof currentLine === 'number' ? currentLine : null

  return (
    <div className="sidebar-pane outline-pane">
      <div className="panel-title">
        Scene Outline
        <span className="spacer" />
        {currentScene ? (
          <span className="outline-scene" title="Running scene">
            ▶ {currentScene}
          </span>
        ) : null}
        <span className="scene-counts">{sections.length} section{sections.length === 1 ? '' : 's'}</span>
      </div>
      {sections.length === 0 ? (
        <div className="explorer-empty">No scene outline (empty script)</div>
      ) : (
        <div className="scene-tree outline-body">
          {sections.map((s) => {
            const headingActive = current === s.line
            const headingClass =
              'timeline-section-title outline-label-row' +
              (headingActive ? ' outline-current' : '')
            return (
              <div className="timeline-section" key={'sec-' + s.line}>
              <div
                className={headingClass}
                title={s.label ? 'label *' + s.label : 'prologue'}
                onClick={() => s.label !== null && handleLabel(s.label, s.line)}
                role={s.label !== null ? 'button' : undefined}
                tabIndex={s.label !== null ? 0 : undefined}
                onContextMenu={
                  s.label !== null
                    ? (e) => {
                        e.preventDefault()
                        handleJump(s.label as string, s.line)
                      }
                    : undefined
                }
                onKeyDown={
                  s.label !== null
                    ? (e) => {
                        if (e.key === 'Enter' || e.key === ' ') {
                          e.preventDefault()
                          handleLabel(s.label as string, s.line)
                        }
                      }
                    : undefined
                }
              >
                <span className="outline-label">{s.label !== null ? '*' + s.label : '(prologue)'}</span>
                <span className="spacer" />
                {s.label !== null && onJumpToLabel && (
                  <button
                    type="button"
                    className="outline-jump"
                    title={'Jump running scene to *' + s.label}
                    onClick={(e) => {
                      e.stopPropagation()
                      handleJump(s.label as string, s.line)
                    }}
                  >
                    ▶
                  </button>
                )}
                <span className="scene-counts">
                  {'L' + s.line + ' · ' + s.items.length}
                </span>
              </div>
              {s.items.map((item) => (
                <OutlineRow
                  key={s.line + '-' + item.line + '-' + item.kind}
                  item={item}
                  active={current === item.line}
                />
              ))}
              </div>
            )
          })}
        </div>
      )}
    </div>
  )
}
