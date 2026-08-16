import { useEditor, type SideView } from '../store'

const VIEWS: { id: SideView; label: string; icon: string }[] = [
  { id: 'explorer', label: 'Explorer (assets)', icon: '📁' },
  { id: 'debug', label: 'Run and Debug', icon: '🐞' },
  { id: 'visual', label: 'Visual Preview', icon: '🎬' },
  { id: 'ai', label: 'AI Writer', icon: '✨' },
  { id: 'settings', label: 'Settings', icon: '⚙️' },
]

export function ActivityBar() {
  const sideView = useEditor((s) => s.sideView)
  const setSideView = useEditor((s) => s.setSideView)

  return (
    <nav className="activity-bar" aria-label="Activity bar">
      {VIEWS.map((v) => (
        <button
          key={v.id}
          className={'activity-item ' + (sideView === v.id ? 'active' : '')}
          onClick={() => setSideView(v.id)}
          title={v.label}
          aria-label={v.label}
          aria-current={sideView === v.id ? 'true' : undefined}
        >
          <span className="activity-icon">{v.icon}</span>
        </button>
      ))}
    </nav>
  )
}
