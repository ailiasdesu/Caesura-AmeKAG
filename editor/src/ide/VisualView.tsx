import { useEffect, useState } from 'react'
import type { EngineClient, FrameReply, Live2DModel } from '../lib/rpc'

interface Props {
  client: EngineClient
}

export function VisualView({ client }: Props) {
  const [frame, setFrame] = useState<FrameReply | null>(null)
  const [error, setError] = useState('')
  const [models, setModels] = useState<Live2DModel[]>([])
  const [modelPath, setModelPath] = useState('')
  const [modelMsg, setModelMsg] = useState('')

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
    const t = setInterval(() => void refreshFrame(), 3000)
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

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Visual Preview
        <span className="spacer" />
        <button onClick={() => void refreshFrame()}>Capture</button>
      </div>

      {frame?.png ? (
        <img
          className="frame-img"
          src={`data:image/png;base64,${frame.png}`}
          alt="Engine frame"
        />
      ) : (
        <div className="frame-empty">{error || 'No frame yet'}</div>
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
