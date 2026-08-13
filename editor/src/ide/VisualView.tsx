import { useEffect, useState } from 'react'
import type { EngineClient, FrameReply, Live2DModel, StateReply } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
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
          />
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
