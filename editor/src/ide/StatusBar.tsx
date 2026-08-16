import { useEditor } from '../store'

/** Max scene-name length shown in the status line before truncation. */
const MAX_SCENE_LEN = 40

/** Truncate a scene path if it overflows the single-line status bar. */
function displayScene(scene: string): string {
  if (!scene) return '—'
  return scene.length > MAX_SCENE_LEN ? scene.slice(0, MAX_SCENE_LEN) + '…' : scene
}

export function StatusBar() {
  const connected = useEditor((s) => s.engineConnected)
  const scene = useEditor((s) => s.engineScene)
  const token = useEditor((s) => s.engineToken)
  const paused = useEditor((s) => s.enginePaused)
  const currentCmd = useEditor((s) => s.engineCmd)

  return (
    <footer className="status-bar">
      <span className={`status-item status-engine ${connected ? 'ok' : 'bad'}`}>
        {connected ? 'Engine: connected' : 'Engine: offline'}
      </span>
      <span className="status-item" title={scene || ''}>
        scene: {displayScene(scene)}
      </span>
      <span className="status-item">token: {token}</span>
      <span className="status-item">cmd: {currentCmd || '—'}</span>
      <span className={`status-item ${paused ? 'paused' : ''}`}>
        {paused ? '⏸ paused' : '▶ running'}
      </span>
      <span className="status-spacer" />
      <span className="status-item">Caesura Editor</span>
    </footer>
  )
}
