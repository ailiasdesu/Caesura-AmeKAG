import { useEffect, useState } from 'react'
import type {
  EngineClient,
  FrameReply,
  Live2DModel,
  PickHit,
  SmaSaveReply,
  SmaValidateReply,
  StateReply,
  StatsReply,
} from '../lib/rpc'
import { useEditor } from '../store'
import { SmaSkeletonCanvas } from './SmaSkeletonCanvas'
import type { SmaCanvasBone } from './SmaSkeletonCanvas'

interface Props {
  client: EngineClient
}

interface SmaBone {
  id: number
  parent: number
  pivot?: [number, number]
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
  const [stats, setStats] = useState<StatsReply | null>(null)
  const [statsError, setStatsError] = useState('')
  const [smaPath, setSmaPath] = useState('demo/assets/sma/hero.json')
  const [smaResult, setSmaResult] = useState<SmaValidateReply | null>(null)
  const [smaError, setSmaError] = useState('')
  const [smaBusy, setSmaBusy] = useState(false)
  // -- SMA editor (round 26) --
  const [draft, setDraft] = useState('')
  const [draftError, setDraftError] = useState('')
  const [editMsg, setEditMsg] = useState('')
  const [saveResult, setSaveResult] = useState<SmaSaveReply | null>(null)
  const [editBusy, setEditBusy] = useState(false)
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

  const refreshStats = async () => {
    setStatsError('')
    try {
      const s = await client.stats()
      if (s.status === 'ok') setStats(s)
      else setStatsError(s.error ?? 'stats unavailable')
    } catch (e) {
      setStatsError(e instanceof Error ? e.message : String(e))
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

  // -- SMA editor (round 26) -----------------------------------------
  // Minimal hero sample used when the engine read channel is unavailable.
  const HERO_SAMPLE = JSON.stringify(
    {
      name: 'hero',
      bones: [
        { id: 0, parent: -1, name: 'root' },
        { id: 1, parent: 0, name: 'torso' },
        { id: 2, parent: 1, name: 'head' },
      ],
      anims: ['idle', 'walk'],
      parts: 2,
      verts: 120,
      tris: 180,
      boneTree: [
        { id: 0, parent: -1 },
        { id: 1, parent: 0 },
        { id: 2, parent: 1 },
      ],
      animDetails: [
        { name: 'idle', duration: 2.0, tracks: [0, 1] },
        { name: 'walk', duration: 1.2, tracks: [0, 1, 2] },
      ],
    },
    null,
    2,
  )

  const loadForEditing = async () => {
    setEditBusy(true)
    setEditMsg('')
    setSaveResult(null)
    setDraftError('')
    const path = smaPath.trim() || 'demo/assets/sma/hero.json'
    try {
      // Prefer reading the asset through the engine's Lua channel (/api/eval
      // returns a value; /api/run alone has no return). Falls back below.
      const script =
        `local f=io.open('${path}','r') if not f then return 'ERR:open' end local t=f:read('*a') f:close() return t`
      const raw = await client.evalRaw(script)
      // The engine JSON-escapes the Lua return; unescape once.
      let content = raw
      try {
        content = JSON.parse(raw) as string
      } catch {
        /* already plain text */
      }
      const trimmed = content.trim()
      if (trimmed === '' || trimmed.startsWith('ERR:open')) {
        throw new Error(trimmed === '' ? 'file empty' : trimmed)
      }
      // Ensure it is a valid JSON document before populating the editor.
      JSON.parse(content)
      setDraft(content)
      setEditMsg(`Loaded ${path} from the engine`)
    } catch (e) {
      // Engine read unavailable -> edit a prefilled sample instead.
      setDraft(HERO_SAMPLE)
      setEditMsg(
        `Engine read unavailable (${e instanceof Error ? e.message : String(e)}); editing a sample draft.`,
      )
    } finally {
      setEditBusy(false)
    }
  }

  const saveDraft = async () => {
    setSaveResult(null)
    setDraftError('')
    setEditMsg('')
    const path = smaPath.trim() || 'demo/assets/sma/hero.json'
    // Local JSON syntax check before hitting the engine.
    try {
      JSON.parse(draft)
    } catch (e) {
      setDraftError(e instanceof Error ? `Invalid JSON: ${e.message}` : 'Invalid JSON')
      return
    }
    setEditBusy(true)
    try {
      const r = await client.smaSave(path, draft)
      setSaveResult(r)
    } catch (e) {
      setEditMsg(e instanceof Error ? e.message : String(e))
    } finally {
      setEditBusy(false)
    }
  }

  // -- SMA skeleton canvas (round 27) --
  // Parse the draft JSON and build {id,parent,pivot} bones for the canvas.
  const draftBones = (): SmaCanvasBone[] | null => {
    try {
      const parsed = JSON.parse(draft) as {
        bones?: { id?: number; parent?: number; pivot?: [number, number] }[]
      }
      const raw = Array.isArray(parsed.bones) ? parsed.bones : []
      const bones: SmaCanvasBone[] = raw
        .filter((b) => typeof b.id === 'number')
        .map((b) => ({
          id: b.id as number,
          parent: typeof b.parent === 'number' ? (b.parent as number) : -1,
          pivot:
            Array.isArray(b.pivot) &&
            typeof b.pivot[0] === 'number' &&
            typeof b.pivot[1] === 'number'
              ? ([Number(b.pivot[0]), Number(b.pivot[1])] as [number, number])
              : ([0.5, 0.5] as [number, number]),
        }))
      return bones
    } catch {
      return null
    }
  }

  // Write updated pivots back into the draft JSON text.
  const applyBonePivots = (bones: SmaCanvasBone[]) => {
    setDraftError('')
    try {
      const parsed = JSON.parse(draft) as Record<string, unknown>
      const raw = Array.isArray((parsed as { bones?: unknown }).bones)
        ? ((parsed as { bones: unknown[] }).bones as unknown[])
        : []
      const pivotMap = new Map<number, [number, number]>()
      for (const b of bones) pivotMap.set(b.id, b.pivot)
      for (const b of raw) {
        const rec = b as { id?: unknown; pivot?: unknown }
        if (typeof rec.id === 'number' && pivotMap.has(rec.id)) {
          rec.pivot = pivotMap.get(rec.id)!
        }
      }
      setDraft(JSON.stringify(parsed, null, 1))
    } catch {
      // JSON became invalid mid-edit; leave the textarea untouched.
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
    void refreshStats()
    const t = setInterval(() => {
      void refreshFrame()
      void refreshState()
      void refreshStats()
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
          <div className="state-row"><span>exec</span><b>{state.current_cmd || '—'}</b></div>
          <div className="state-row"><span>language</span><b>{state.language || '-'}</b></div>
          <div className="state-row"><span>nvl</span><b>{state.nvl_mode ? 'on' : 'off'}</b></div>
          <div className="state-row"><span>backlog</span><b>{state.backlog_count ?? 0}</b></div>
          <div className="state-row"><span>layers</span><b>{state.layer_count ?? 0}</b></div>
        </div>
      ) : (
        <div className="frame-empty">No engine state yet</div>
      )}

      <div className="panel-subtitle">
        ENGINE STATS
        <span className="spacer" />
        <button onClick={() => void refreshStats()}>Refresh</button>
      </div>
      {statsError ? (
        <div className="panel-msg">{statsError}</div>
      ) : stats ? (
        <div className="state-grid">
          <div className="state-row">
            <span>texture budget</span>
            <b>{stats.texture_budget_mb != null ? stats.texture_budget_mb + ' MB' : '-'}{stats.texture_tier_name ? ' · ' + stats.texture_tier_name : ''}</b>
          </div>
          <div className="state-row"><span>mesh count</span><b>{stats.mesh_count ?? 0}</b></div>
          <div className="state-row"><span>job workers</span><b>{stats.job_workers ?? 0}</b></div>
          <div className="state-row"><span>job pending</span><b>{stats.job_pending ?? 0}</b></div>
          <div className="state-row">
            <span>Lua heap</span>
            <b>{stats.lua_kb != null ? stats.lua_kb + ' KB' : '-'}</b>
          </div>
        </div>
      ) : (
        <div className="frame-empty">No engine stats yet</div>
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

      <div className="panel-subtitle">
        SMA EDITOR
        <span className="spacer" />
        <button onClick={() => void loadForEditing()} disabled={editBusy || smaBusy}>
          {editBusy ? 'Loading…' : 'Load for editing'}
        </button>
      </div>
      <div className="sma-canvas-block">
        <div className="sma-canvas-title">Skeleton (click/drag to edit pivots)</div>
        {draftBones() ? (
          <SmaSkeletonCanvas bones={draftBones()!} onBonesChange={applyBonePivots} />
        ) : (
          <div className="sma-canvas-empty">(JSON parsing failed, skeleton cannot be edited)</div>
        )}
      </div>
      <textarea
        className="sma-editor"
        value={draft}
        onChange={(e) => {
          setDraft(e.target.value)
          setDraftError('')
        }}
        placeholder={'{\n  // Edit hero.json here (JSON)\n}'}
        spellCheck={false}
      />
      {draftError && <div className="panel-msg sma-bad-msg">{draftError}</div>}
      {editMsg && <div className="panel-msg">{editMsg}</div>}
      <div className="bp-row">
        <button
          className="sma-save"
          onClick={() => void saveDraft()}
          disabled={editBusy || smaBusy || draft.trim() === ''}
        >
          {editBusy ? 'Saving…' : 'Save'}
        </button>
        <span className="editor-hint">validates JSON locally, then writes via /api/sma/save</span>
      </div>
      {saveResult && (
        <div className={saveResult.ok ? 'sma-result sma-ok' : 'sma-result sma-bad'}>
          <div className="state-row">
            <span>save</span>
            <b>{saveResult.ok ? '✓ saved' : '✗ failed'}</b>
          </div>
          {!saveResult.ok && saveResult.errors.length > 0 && (
            <ul className="sma-errors">
              {saveResult.errors.map((err, i) => (
                <li key={i}>{err}</li>
              ))}
            </ul>
          )}
        </div>
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