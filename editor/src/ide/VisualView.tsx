import { useEffect, useState } from 'react'
import type {
  EngineClient,
  FrameReply,
  Live2DModel,
  PickHit,
  SmaValidateReply,
  StateReply,
} from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

interface SmaBone {
  id: number
  parent: number
}

interface SmaAnimDetail {
  name: string
  duration: number
  tracks: number[]
}

interface SmaMeta {
  bones: number
  anims: string[]
  parts: number
  verts: number
  tris: number
  boneTree?: SmaBone[]
  animDetails?: SmaAnimDetail[]
}

function SkeletonTree({ bones }: { bones: SmaBone[] }) {
  // Build children maps; root = parent -1 (or an id missing from parents).
  const childrenOf = new Map<number, SmaBone[]>()
  const roots: SmaBone[] = []
  const known = new Set(bones.map((b) => b.id))
  for (const b of bones) {
    if (b.parent === -1 || !known.has(b.parent)) {
      roots.push(b)
    } else {
      const list = childrenOf.get(b.parent) ?? []
      list.push(b)
      childrenOf.set(b.parent, list)
    }
  }
  const renderBone = (b: SmaBone, depth: number): JSX.Element[] => {
    const kids = childrenOf.get(b.id) ?? []
    return [
      <div className="state-row" key={b.id} style={{ paddingLeft: depth * 10 }}>
        <span>◇ bone {b.id}</span>
        <b>{kids.length > 0 ? `${kids.length} child` : 'leaf'}</b>
      </div>,
      ...kids.flatMap((k) => renderBone(k, depth + 1)),
    ]
  }
  return <>{roots.flatMap((r) => renderBone(r, 0))}</>
}

function SmaMetaView({ metaText }: { metaText: string }) {
  let meta: SmaMeta = { bones: 0, anims: [], parts: 0, verts: 0, tris: 0 }
  try {
    meta = JSON.parse(metaText) as SmaMeta
  } catch {
    /* keep defaults */
  }
  return (
    <>
      <div className="state-row"><span>bones</span><b>{meta.bones}</b></div>
      <div className="state-row">
        <span>anims</span>
        <b>{meta.anims.length > 0 ? meta.anims.join(', ') : '-'}</b>
      </div>
      <div className="state-row"><span>parts</span><b>{meta.parts}</b></div>
      <div className="state-row">
        <span>verts/tris</span>
        <b>{meta.verts}/{meta.tris}</b>
      </div>
      {meta.boneTree && meta.boneTree.length > 0 && (
        <div className="sma-tree">
          <div className="sma-tree-title">Skeleton</div>
          <SkeletonTree bones={meta.boneTree} />
        </div>
      )}
      {meta.animDetails && meta.animDetails.length > 0 && (
        <div className="sma-tree">
          <div className="sma-tree-title">Animations</div>
          {meta.animDetails.map((d) => (
            <div className="state-row" key={d.name}>
              <span>{d.name} ({d.duration}s)</span>
              <b>tracks: {d.tracks.join(', ') || '-'}</b>
            </div>
          ))}
        </div>
      )}
    </>
  )
}

export function VisualView({ client }: Props) {
  const [frame, setFrame] = useState<FrameReply | null>(null)
  const [error, setError] = useState('')
  const [models, setModels] = useState<Live2DModel[]>([])
  const [modelPath, setModelPath] = useState('')
  const [modelMsg, setModelMsg] = useState('')
  const [dragOver, setDragOver] = useState(false)
  const [state, setState] = useState<StateReply | null>(null)
  const [stateError, setStateError] = useState('')
  const [smaPath, setSmaPath] = useState('demo/assets/sma/hero.json')
  const [smaResult, setSmaResult] = useState<SmaValidateReply | null>(null)
  const [smaError, setSmaError] = useState('')
  const [smaBusy, setSmaBusy] = useState(false)
  const [pickHits, setPickHits] = useState<PickHit[]>([])
  const [pickMsg, setPickMsg] = useState('')
  const insertIntoActive = useEditor((s) => s.insertIntoActive)

  const refreshState = async () => {
    setStateError('')
    try {
      const s = await client.state()
      if (s.status === 'ok') setState(s)
      else setStateError(s.error ?? 'state unavailable')
    } catch (e) {
      setStateError(e instanceof Error ? e.message : String(e))
    }
  }

  const refreshFrame = async () => {
    setError('')
    try {
      const f = await client.frame(640, 360)
      if (f.status === 'ok' && f.png) setFrame(f)
      else setError(f.error ?? 'frame capture unavailable')
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    }
  }

  const refreshSma = async () => {
    setSmaError('')
    setSmaResult(null)
    setSmaBusy(true)
    try {
      const s = await client.smaValidate(smaPath.trim())
      setSmaResult(s)
    } catch (err) {
      setSmaError(err instanceof Error ? err.message : String(err))
    } finally {
      setSmaBusy(false)
    }
  }

  const onFrameClick = async (e: React.MouseEvent<HTMLImageElement>) => {
    setPickMsg('')
    const rect = e.currentTarget.getBoundingClientRect()
    if (rect.width <= 0 || rect.height <= 0) return
    // Engine frame space is 1280x720; the preview is scaled down.
    const x = Math.round(((e.clientX - rect.left) / rect.width) * 1280)
    const y = Math.round(((e.clientY - rect.top) / rect.height) * 720)
    try {
      const r = await client.pick(x, y)
      let hits: PickHit[] = []
      try {
        hits = JSON.parse(r.hits) as PickHit[]
      } catch {
        hits = []
      }
      setPickHits(hits)
      setPickMsg(hits.length === 0 ? '(no layer at this pixel)' : '')
    } catch (err) {
      setPickMsg(err instanceof Error ? err.message : String(err))
    }
  }

  const refreshModels = async () => {
    try {
      const ms = await client.live2dModels()
      setModels(ms)
      if (ms.length > 0 && !modelPath) setModelPath(ms[0].path)
    } catch {
      /* engine without Live2D — leave list empty */
    }
  }

  useEffect(() => {
    void refreshModels()
    void refreshFrame()
    void refreshState()
    const t = setInterval(() => {
      void refreshFrame()
      void refreshState()
    }, 3000)
    return () => clearInterval(t)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [client])

  const loadModel = async () => {
    setModelMsg('')
    try {
      const r = await client.live2dLoad(modelPath)
      setModelMsg(r.status === 'ok' ? `Model loaded (id ${r.modelId})` : r.status)
    } catch (e) {
      setModelMsg(e instanceof Error ? e.message : String(e))
    }
  }

  // -- Battle 4b: drag-drop asset -> generated tag into the active doc --
  const onDrop = (e: React.DragEvent) => {
    e.preventDefault()
    setDragOver(false)
    const raw = e.dataTransfer.getData('application/x-caesura-asset')
    if (!raw) return
    const asset = JSON.parse(raw) as { path: string; type: string }
    const storage = asset.path.replace(/^assets\//, '')
    let tag: string
    if (asset.type === 'image') {
      tag = `[bg storage="${storage}"]\n`
    } else if (asset.type === 'audio') {
      tag = `[playbgm file="${storage}"]\n`
    } else {
      tag = `[ch text="${asset.path}"]\n`
    }
    insertIntoActive(tag)
    setModelMsg(`Inserted: ${tag.trim()}`)
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Visual Preview
        <span className="spacer" />
        <button onClick={() => void refreshFrame()}>Capture</button>
      </div>

      {frame?.png ? (
        <div
          className={`drop-zone ${dragOver ? 'drag-over' : ''}`}
          onDragOver={(e) => {
            e.preventDefault()
            setDragOver(true)
          }}
          onDragLeave={() => setDragOver(false)}
          onDrop={onDrop}
        >
          <img
            className="frame-img"
            src={`data:image/png;base64,${frame.png}`}
            alt="Engine frame"
            onClick={(ev) => void onFrameClick(ev)}
            style={{ cursor: 'crosshair' }}
          />
          {pickMsg && <div className="panel-msg">{pickMsg}</div>}
          {pickHits.length > 0 && (
            <div className="state-grid">
              {pickHits.map((h, i) => (
                <div className="state-row" key={i}>
                  <span>{h.name || h.id} (z={h.z})</span>
                  <b>x{h.x},y{h.y} {h.w}×{h.h}</b>
                </div>
              ))}
            </div>
          )}
          <div className="drop-hint">
            Drop images/audio here → inserts [bg]/[playbgm] into the active script
          </div>
        </div>
      ) : (
        <div className="frame-empty">{error || 'No frame yet'}</div>
      )}

      <div className="panel-subtitle">
        ENGINE STATE
        <span className="spacer" />
        <button onClick={() => void refreshState()}>Refresh</button>
      </div>
      {stateError ? (
        <div className="panel-msg">{stateError}</div>
      ) : state ? (
        <div className="state-grid">
          <div className="state-row"><span>scene</span><b>{state.scene || '(none)'}</b></div>
          <div className="state-row"><span>token</span><b>{state.token_index ?? 0}</b></div>
          <div className="state-row"><span>language</span><b>{state.language || '-'}</b></div>
          <div className="state-row"><span>nvl</span><b>{state.nvl_mode ? 'on' : 'off'}</b></div>
          <div className="state-row"><span>backlog</span><b>{state.backlog_count ?? 0}</b></div>
          <div className="state-row"><span>layers</span><b>{state.layer_count ?? 0}</b></div>
        </div>
      ) : (
        <div className="frame-empty">No engine state yet</div>
      )}

      <div className="panel-subtitle">
        SMA ASSET
        <span className="spacer" />
        <button onClick={() => void refreshSma()} disabled={smaBusy}>
          {smaBusy ? 'Validating…' : 'Validate'}
        </button>
      </div>
      <div className="bp-row">
        <input
          className="model-select"
          value={smaPath}
          onChange={(e) => setSmaPath(e.target.value)}
          placeholder="demo/assets/sma/hero.json"
        />
      </div>
      {smaError ? (
        <div className="panel-msg">{smaError}</div>
      ) : smaResult ? (
        <div className={smaResult.ok ? 'sma-result sma-ok' : 'sma-result sma-bad'}>
          <div className="state-row">
            <span>status</span>
            <b>{smaResult.ok ? '✓ valid' : '✗ invalid'}</b>
          </div>
          {smaResult.ok && <SmaMetaView metaText={smaResult.meta} />}
          {!smaResult.ok && smaResult.errors.length > 0 && (
            <ul className="sma-errors">
              {smaResult.errors.map((err, i) => (
                <li key={i}>{err}</li>
              ))}
            </ul>
          )}
        </div>
      ) : (
        <div className="frame-empty">Enter a path and validate</div>
      )}

      <div className="panel-subtitle">LIVE2D</div>
      <div className="bp-row">
        <select
          className="model-select"
          value={modelPath}
          onChange={(e) => setModelPath(e.target.value)}
        >
          {models.length === 0 && <option value="">(no models)</option>}
          {models.map((m) => (
            <option key={m.path} value={m.path}>
              {m.name}
            </option>
          ))}
        </select>
        <button onClick={() => void loadModel()}>Load</button>
      </div>
      {modelMsg && <div className="panel-msg">{modelMsg}</div>}
    </div>
  )
}
