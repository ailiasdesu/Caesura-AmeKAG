// G4 editor — scene outline panel (virtualized).
//
// Read-only outline of a .ks scene: *labels as section headings, [command...]
// tags as command rows (command name + key params), and bare text lines as
// content. Presentational only — takes the scene text and an optional
// label-click callback; the caller (ExplorerView / a future store wiring)
// supplies the source and handles navigation.
//
// Long-scene performance: the sections are flattened into one linear row list
// and only the rows near the scroll window are mounted (fixed row height +
// scrollTop-derived viewport). Label click navigation, per-section jump
// buttons, scene-name display and outline-current highlighting are preserved.

import { useMemo, useState, useRef, useLayoutEffect, useCallback } from 'react'
import {
  parseSceneOutline,
  buildOutlineSections,
  flattenOutlineRows,
  getVisibleWindow,
  scrollTopToRevealRow,
  flatRowLine,
  OUTLINE_ROW_HEIGHT,
  OUTLINE_OVERSCAN,
  DEFAULT_OUTLINE_VIEWPORT,
} from '../lib/sceneOutline'
import type { OutlineItem, OutlineSection } from './sceneOutlineTypes'
import type { OutlineFlatRow } from '../lib/sceneOutline'

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
  /** Fixed pixel height of one row. Defaults to OUTLINE_ROW_HEIGHT. */
  rowHeight?: number
  /** Scrolling viewport height (px). When omitted (0/undefined) the component
   *  measures its own scroll container at runtime, falling back to
   *  DEFAULT_OUTLINE_VIEWPORT when the DOM reports no height (jsdom / SSR). */
  viewportHeight?: number
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

interface SectionHeadingProps {
  section: OutlineSection
  active: boolean
  onSelectLabel?: (label: string, line: number) => void
  onJumpToLabel?: (label: string, line: number) => void
}

/** A *label section heading row (kept as one virtual row). */
function SectionHeading({
  section,
  active,
  onSelectLabel,
  onJumpToLabel,
}: SectionHeadingProps) {
  const { label, line, items } = section
  const headingClass =
    'timeline-section-title outline-label-row' + (active ? ' outline-current' : '')

  const handleLabel = (label: string, line: number) => {
    if (onSelectLabel) onSelectLabel(label, line)
  }
  const handleJump = (label: string, line: number) => {
    if (onJumpToLabel) onJumpToLabel(label, line)
  }

  return (
    <div
      className={headingClass}
      title={label !== null ? 'label *' + label : 'prologue'}
      onClick={() => label !== null && handleLabel(label, line)}
      role={label !== null ? 'button' : undefined}
      tabIndex={label !== null ? 0 : undefined}
      onContextMenu={
        label !== null
          ? (e) => {
              e.preventDefault()
              handleJump(label as string, line)
            }
          : undefined
      }
      onKeyDown={
        label !== null
          ? (e) => {
              if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault()
                handleLabel(label as string, line)
              }
            }
          : undefined
      }
    >
      <span className="outline-label">{label !== null ? '*' + label : '(prologue)'}</span>
      <span className="spacer" />
      {label !== null && onJumpToLabel && (
        <button
          type="button"
          className="outline-jump"
          title={'Jump running scene to *' + label}
          onClick={(e) => {
            e.stopPropagation()
            handleJump(label as string, line)
          }}
        >
          ▶
        </button>
      )}
      <span className="scene-counts">{'L' + line + ' · ' + items.length}</span>
    </div>
  )
}

export function SceneOutline({
  source,
  onSelectLabel,
  onJumpToLabel,
  currentLine,
  currentScene,
  rowHeight,
  viewportHeight,
}: SceneOutlineProps) {
  const rowH = rowHeight && rowHeight > 0 ? rowHeight : OUTLINE_ROW_HEIGHT

  const sections = useMemo(
    () => buildOutlineSections(parseSceneOutline(source)),
    [source],
  )
  const rows = useMemo(() => flattenOutlineRows(sections), [sections])
  // The row currently at the live engine position (a single source line maps
  // to at most one outline row — a section heading OR an item row).
  const current = typeof currentLine === 'number' ? currentLine : null

  const currentRowIndex = useMemo(() => {
    if (current == null) return -1
    return rows.findIndex((r) => flatRowLine(r) === current)
  }, [rows, current])

  // Scroll container + live viewport measurement. When a viewportHeight prop is
  // provided it wins (so tests can drive the window without DOM layout); else
  // measure the container's clientHeight (ResizeObserver when available),
  // falling back to a constant when the DOM reports no height.
  const scrollRef = useRef<HTMLDivElement | null>(null)
  const [scrollTop, setScrollTop] = useState(0)
  const [measuredHeight, setMeasuredHeight] = useState<number | null>(null)

  const containerRef = useCallback((el: HTMLDivElement | null) => {
    scrollRef.current = el
    if (el) setMeasuredHeight((prev) => (el.clientHeight > 0 ? el.clientHeight : prev))
  }, [])

  useLayoutEffect(() => {
    const el = scrollRef.current
    if (!el) return
    if (el.clientHeight > 0) setMeasuredHeight(el.clientHeight)
    if (typeof ResizeObserver === 'undefined') return
    const ro = new ResizeObserver(() => {
      setMeasuredHeight(scrollRef.current?.clientHeight ?? 0)
    })
    ro.observe(el)
    return () => ro.disconnect()
  }, [])

  const effectiveViewport =
    viewportHeight != null && viewportHeight > 0
      ? viewportHeight
      : measuredHeight && measuredHeight > 0
        ? measuredHeight
        : DEFAULT_OUTLINE_VIEWPORT

  const win = useMemo(
    () =>
      getVisibleWindow({
        scrollTop,
        viewportHeight: effectiveViewport,
        rowHeight: rowH,
        totalRows: rows.length,
        overscan: OUTLINE_OVERSCAN,
      }),
    [scrollTop, effectiveViewport, rowH, rows.length],
  )

  // Bring the highlight row into view when the engine position changes. A ref
  // guard makes this fire once per distinct target row, so it never fights a
  // deliberate user scroll after the reveal has happened.
  const lastRevealedRef = useRef(-2)
  useLayoutEffect(() => {
    if (currentRowIndex < 0) {
      lastRevealedRef.current = -2
      return
    }
    if (lastRevealedRef.current === currentRowIndex) return
    lastRevealedRef.current = currentRowIndex
    const target = scrollTopToRevealRow({
      rowIndex: currentRowIndex,
      rowHeight: rowH,
      viewportHeight: effectiveViewport,
      currentScrollTop: scrollTop,
      totalRows: rows.length,
    })
    if (target !== scrollTop) setScrollTop(target)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [currentRowIndex, rowH, effectiveViewport, rows.length])

  const handleScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    setScrollTop(e.currentTarget.scrollTop)
  }, [])

  const visibleRows = rows.slice(win.startIndex, win.endIndex + 1)

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
      {rows.length === 0 ? (
        <div className="explorer-empty">No scene outline (empty script)</div>
      ) : (
        <div
          className="scene-tree outline-body"
          ref={containerRef}
          onScroll={handleScroll}
        >
          {win.paddingTop > 0 && (
            <div className="outline-spacer" style={{ height: win.paddingTop }} />
          )}
          {visibleRows.map((row) => (
            <VirtualRow
              key={row.key}
              row={row}
              current={current}
              onSelectLabel={onSelectLabel}
              onJumpToLabel={onJumpToLabel}
            />
          ))}
          {win.paddingBottom > 0 && (
            <div className="outline-spacer" style={{ height: win.paddingBottom }} />
          )}
        </div>
      )}
    </div>
  )
}

/** Render one flattened outline row (section heading or item) keyed for the
 *  virtual list. Keeps the current-row highlight and both click affordances. */
function VirtualRow({
  row,
  current,
  onSelectLabel,
  onJumpToLabel,
}: {
  row: OutlineFlatRow
  current: number | null
  onSelectLabel?: (label: string, line: number) => void
  onJumpToLabel?: (label: string, line: number) => void
}) {
  if (row.rowKind === 'section') {
    return (
      <SectionHeading
        section={row.section}
        active={current === row.section.line}
        onSelectLabel={onSelectLabel}
        onJumpToLabel={onJumpToLabel}
      />
    )
  }
  return (
    <OutlineRow item={row.item} active={current === row.item.line} />
  )
}
