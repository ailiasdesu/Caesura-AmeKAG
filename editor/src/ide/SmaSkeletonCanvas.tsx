import { useEffect, useRef, useState } from 'react'

export interface SmaCanvasBone {
  id: number
  parent: number
  pivot: [number, number]
}

interface Props {
  bones: SmaCanvasBone[]
  onBonesChange?: (bones: SmaCanvasBone[]) => void
  width?: number
  height?: number
}

// Pivot coordinates live in [0,1] space with y pointing down; map into a DPR
// aware canvas pixel grid. Root bones (parent === -1) are laid out along the
// top edge so the hierarchy reads top-down.
export function SmaSkeletonCanvas({
  bones,
  onBonesChange,
  width = 220,
  height = 220,
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null)
  const [size, setSize] = useState({ width, height })
  const [selected, setSelected] = useState<number | null>(null)
  const dragRef = useRef<{ id: number } | null>(null)
  const bonesRef = useRef(bones)
  bonesRef.current = bones

  // Apply effective size; keep internal coordinate space consistent.
  useEffect(() => {
    setSize((s) =>
      s.width === width && s.height === height ? s : { width, height },
    )
  }, [width, height])

  const clamp01 = (v: number) => Math.min(1, Math.max(0, v))

  const draw = () => {
    const canvas = canvasRef.current
    if (!canvas) return
    const dpr = typeof window !== 'undefined' ? window.devicePixelRatio || 1 : 1
    canvas.width = Math.max(1, Math.round(size.width * dpr))
    canvas.height = Math.max(1, Math.round(size.height * dpr))
    const g = canvas.getContext('2d')
    if (!g) return
    g.setTransform(dpr, 0, 0, dpr, 0, 0)
    g.clearRect(0, 0, size.width, size.height)

    const px = (p: [number, number]): [number, number] => [
      p[0] * size.width,
      p[1] * size.height,
    ]
    const pos = new Map<number, [number, number]>()
    for (const b of bones) pos.set(b.id, px(b.pivot))

    // Connections: from parent pivot to child pivot.
    g.lineWidth = 1
    for (const b of bones) {
      if (b.parent === -1) continue
      const child = pos.get(b.id)
      const par = pos.get(b.parent)
      if (!child || !par) continue
      g.strokeStyle = 'rgba(139,148,158,0.55)'
      g.beginPath()
      g.moveTo(par[0], par[1])
      g.lineTo(child[0], child[1])
      g.stroke()
    }

    // Nodes: filled circle + id label; highlight the selection.
    for (const b of bones) {
      const [x, y] = pos.get(b.id) ?? [0, 0]
      const isSel = b.id === selected
      g.beginPath()
      g.arc(x, y, isSel ? 6 : 4.5, 0, Math.PI * 2)
      g.fillStyle = isSel ? '#58a6ff' : '#e6edf3'
      g.fill()
      g.strokeStyle = isSel ? '#1f6feb' : '#30363d'
      g.lineWidth = 1
      g.stroke()

      g.fillStyle = isSel ? '#58a6ff' : 'rgba(230,237,243,0.75)'
      g.font = '10px "Cascadia Code", "Consolas", monospace'
      g.fillText(String(b.id), x + 8, y - 5)
    }
  }

  useEffect(() => {
    draw()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [bones, size, selected])

  const pivotAt = (e: React.MouseEvent): [number, number] => {
    const rect = canvasRef.current?.getBoundingClientRect()
    if (!rect || rect.width <= 0 || rect.height <= 0) return [0, 0]
    const x = clamp01((e.clientX - rect.left) / rect.width)
    const y = clamp01((e.clientY - rect.top) / rect.height)
    return [x, y]
  }

  // Nearest bone within a 10px radius (pixel-space distance).
  const nearest = (p: [number, number]): number | null => {
    let best: number | null = null
    let bestPx = Infinity
    for (const b of bonesRef.current) {
      const dxPx = (b.pivot[0] - p[0]) * size.width
      const dyPx = (b.pivot[1] - p[1]) * size.height
      const px = Math.sqrt(dxPx * dxPx + dyPx * dyPx)
      if (px <= 10 && px < bestPx) {
        bestPx = px
        best = b.id
      }
    }
    return best
  }

  const updatePivot = (id: number, pivot: [number, number]) => {
    const next = bonesRef.current.map((b) =>
      b.id === id ? { ...b, pivot } : b,
    )
    onBonesChange?.(next)
  }

  const onMouseDown = (e: React.MouseEvent) => {
    const p = pivotAt(e)
    const id = nearest(p)
    setSelected(id)
    if (id !== null) {
      dragRef.current = { id }
      e.preventDefault()
    }
  }

  const onMouseMove = (e: React.MouseEvent) => {
    const drag = dragRef.current
    if (!drag) return
    const p = pivotAt(e)
    updatePivot(drag.id, [p[0], p[1]])
  }

  const stopDrag = () => {
    dragRef.current = null
  }

  const onNumberChange = (axis: 0 | 1) => (ev: React.ChangeEvent<HTMLInputElement>) => {
    if (selected === null) return
    const b = bonesRef.current.find((x) => x.id === selected)
    if (!b) return
    let raw = Number(ev.target.value)
    if (Number.isNaN(raw)) raw = 0
    const pivot: [number, number] = axis === 0 ? [clamp01(raw), b.pivot[1]] : [b.pivot[0], clamp01(raw)]
    updatePivot(b.id, pivot)
  }

  const sel = selected !== null ? bonesRef.current.find((b) => b.id === selected) : null

  return (
    <div className="sma-canvas">
      <canvas
        ref={canvasRef}
        width={size.width}
        height={size.height}
        style={{ width: size.width, height: size.height, display: 'block' }}
        onMouseDown={onMouseDown}
        onMouseMove={onMouseMove}
        onMouseUp={stopDrag}
        onMouseLeave={stopDrag}
      />
      {bones.length === 0 && (
        <div className="sma-canvas-empty">(no bones)</div>
      )}
      {sel && (
        <div className="sma-canvas-pivot">
          <span>pivot #{sel.id}</span>
          <label>
            x
            <input
              type="number"
              min={0}
              max={1}
              step={0.01}
              value={Number(sel.pivot[0].toFixed(2))}
              onChange={onNumberChange(0)}
            />
          </label>
          <label>
            y
            <input
              type="number"
              min={0}
              max={1}
              step={0.01}
              value={Number(sel.pivot[1].toFixed(2))}
              onChange={onNumberChange(1)}
            />
          </label>
        </div>
      )}
    </div>
  )
}

export default SmaSkeletonCanvas
