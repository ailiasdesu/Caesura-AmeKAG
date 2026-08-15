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
}

/** Render one outline row: a command row or a text content line. */
function OutlineRow({ item }: { item: OutlineItem }) {
  if (item.kind === 'command') {
    const keyParams = Object.entries(item.params)
      .slice(0, 3)
      .map(([k, v]) => k + '=' + v)
      .join(' ')
    const cmdText = '[' + item.cmd + ']'
    return (
      <div
        className="outline-row outline-command"
        title={'line ' + item.line + ' — ' + cmdText}
      >
        <span className="outline-line">{item.line}</span>
        <span className="outline-cmd">{cmdText}</span>
        {keyParams && <span className="outline-cmd-params">{keyParams}</span>}
      </div>
    )
  }
  if (item.kind !== 'text') return null
  return (
    <div className="outline-row outline-text" title={'line ' + item.line}>
      <span className="outline-line">{item.line}</span>
      <span className="outline-content">{item.content}</span>
    </div>
  )
}

export function SceneOutline({ source, onSelectLabel, onJumpToLabel }: SceneOutlineProps) {
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

  return (
    <div className="sidebar-pane outline-pane">
      <div className="panel-title">
        Scene Outline
        <span className="spacer" />
        <span className="scene-counts">{sections.length} section{sections.length === 1 ? '' : 's'}</span>
      </div>
      {sections.length === 0 ? (
        <div className="explorer-empty">No scene outline (empty script)</div>
      ) : (
        <div className="scene-tree outline-body">
          {sections.map((s) => (
            <div className="timeline-section" key={'sec-' + s.line}>
              <div
                className="timeline-section-title outline-label-row"
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
                <OutlineRow key={s.line + '-' + item.line + '-' + item.kind} item={item} />
              ))}
            </div>
          ))}
        </div>
      )}
    </div>
  )
}
