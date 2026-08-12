import { useEditor } from '../store'

export function StatusBar() {
  const connected = useEditor((s) => s.engineConnected)
  const scene = useEditor((s) => s.engineScene)
  const token = useEditor((s) => s.engineToken)
  const paused = useEditor((s) => s.enginePaused)

  return (
    <footer className="status-bar">
      <span className={`status-item status-engine ${connected ? 'ok' : 'bad'}`}>
        {connected ? 'Engine: connected' : 'Engine: offline'}
      </span>
      <span className="status-item">scene: {scene || '—'}</span>
      <span className="status-item">token: {token}</span>
      <span className={`status-item ${paused ? 'paused' : ''}`}>
        {paused ? '⏸ paused' : '▶ running'}
      </span>
      <span className="status-spacer" />
      <span className="status-item">Caesura Editor</span>
    </footer>
  )
}
