import { useCallback, useEffect, useState } from 'react'
import type { AssetEntry, EngineClient } from '../lib/rpc'
import { filterByType, type AssetTypeFilter } from '../lib/assetFilter'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

function langFor(path: string): string {
  return path.toLowerCase().endsWith('.ks') ? 'kag' : 'lua'
}

/** Deterministic palette keyed by asset kind for the thumbnail placeholder. */
const THUMB_COLORS: Record<string, string> = {
  bg: '#7f9cf5',
  fg: '#48bb78',
  char: '#ed8936',
  ui: '#38bdf8',
  bgm: '#a78bfa',
  voice: '#f472b6',
  se: '#f59e0b',
}

function thumbColor(kind?: string): string {
  return (kind && THUMB_COLORS[kind]) || '#6c7086'
}

interface TypeButton {
  value: AssetTypeFilter
  label: string
}

const TYPE_BUTTONS: TypeButton[] = [
  { value: 'all', label: 'All' },
  { value: 'image', label: 'Images' },
  { value: 'audio', label: 'Audio' },
  { value: 'script', label: 'Scripts' },
]

export function ExplorerView({ client }: Props) {
  const [assets, setAssets] = useState<AssetEntry[]>([])
  const [filter, setFilter] = useState('')
  const [selectedType, setSelectedType] = useState<AssetTypeFilter>('all')
  const [error, setError] = useState('')
  const openDoc = useEditor((s) => s.openDoc)
  const docs = useEditor((s) => s.docs)
  const activePath = useEditor((s) => s.activePath)
  const setActive = useEditor((s) => s.setActive)
  const closeDoc = useEditor((s) => s.closeDoc)

  const refresh = useCallback(async () => {
    setError('')
    try {
      setAssets(await client.assets())
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e))
    }
  }, [client])

  useEffect(() => {
    void refresh()
    const t = setInterval(() => void refresh(), 5000)
    return () => clearInterval(t)
  }, [refresh])

  const byName = filter
    ? assets.filter((a) => a.name.toLowerCase().includes(filter.toLowerCase()))
    : assets
  const byType = filterByType(byName, selectedType)

  const scripts = byType.filter((a) => a.type === 'script')
  const images = byType.filter((a) => a.type === 'image')
  const audio = byType.filter((a) => a.type === 'audio')
  const showEmpty = (type: AssetTypeFilter) =>
    selectedType === 'all' || selectedType === type

  const openScript = async (a: AssetEntry) => {
    // Scripts load through the engine eval (sandbox io read) -- the editor
    // shows the file and lets the user edit; content is fetched via eval
    // when the engine supports it, otherwise starts empty with a hint.
    try {
      const content = (await client.inspect(`__editor_read_script(${JSON.stringify(a.path)})`)) as string
      openDoc({ path: a.path, name: a.name, language: langFor(a.path), content: typeof content === 'string' ? content : '', dirty: false })
    } catch {
      openDoc({ path: a.path, name: a.name, language: langFor(a.path), content: '', dirty: false })
    }
  }

  const openImage = (a: AssetEntry) => {
    openDoc({ path: a.path, name: a.name, language: 'plaintext', content: `<!-- image asset: ${a.path} -->\n<!-- preview in the Visual view -->`, dirty: false })
  }

  return (
    <div className="sidebar-pane">
      <div className="panel-title">
        Explorer
        <span className="spacer" />
        <button onClick={() => void refresh()}>↻</button>
      </div>
      <div className="explorer-types-row" role="group" aria-label="Filter by asset type">
        {TYPE_BUTTONS.map((b) => (
          <button
            key={b.value}
            type="button"
            className={`explorer-type-btn ${selectedType === b.value ? 'active' : ''}`}
            aria-pressed={selectedType === b.value}
            style={selectedType === b.value ? {
              background: 'var(--bg-hover)',
              color: 'var(--accent)',
              fontWeight: 600,
              borderColor: 'var(--accent)',
            } : undefined}
            onClick={() => setSelectedType(b.value)}
          >
            {b.label}
          </button>
        ))}
      </div>
      <input
        className="explorer-filter"
        placeholder="Filter assets…"
        value={filter}
        onChange={(e) => setFilter(e.target.value)}
      />

      {docs.length > 0 && (
        <section className="explorer-section">
          <div className="explorer-section-title">OPEN EDITORS</div>
          {docs.map((d) => (
            <div
              key={d.path}
              className={`explorer-item ${d.path === activePath ? 'active' : ''}`}
              onClick={() => setActive(d.path)}
              title={d.path}
            >
              <span className="explorer-file-icon">
                {d.language === 'kag' ? '📜' : '📄'}
              </span>
              <span className="explorer-item-name">{d.name}</span>
              {d.dirty && <span className="dirty-dot">●</span>}
              <button
                className="explorer-close"
                onClick={(e) => {
                  e.stopPropagation()
                  closeDoc(d.path)
                }}
              >
                ✕
              </button>
            </div>
          ))}
        </section>
      )}

      <section className="explorer-section">
        <div className="explorer-section-title">SCRIPTS (.ks)</div>
        {scripts.map((a) => (
          <div
            key={a.path}
            className="explorer-item"
            title={a.path}
            onDoubleClick={() => void openScript(a)}
          >
            <span className="explorer-file-icon">📜</span>
            <span className="explorer-item-name">{a.name}</span>
          </div>
        ))}
        {scripts.length === 0 && showEmpty('script') && (
          <div className="explorer-empty">No scripts found</div>
        )}
      </section>

      <section className="explorer-section">
        <div className="explorer-section-title">IMAGES</div>
        {images.map((a) => (
          <div
            key={a.path}
            className="explorer-item"
            title={a.path}
            draggable
            onDragStart={(e) => {
              e.dataTransfer.setData(
                'application/x-caesura-asset',
                JSON.stringify({ path: a.path, type: a.type }),
              )
              e.dataTransfer.effectAllowed = 'copy'
            }}
            onDoubleClick={() => openImage(a)}
          >
            <span
              className="explorer-thumb"
              style={{ background: thumbColor(a.kind) }}
              aria-hidden="true"
            >
              {a.kind || 'img'}
            </span>
            <span className="explorer-item-name">{a.name}</span>
          </div>
        ))}
        {images.length === 0 && showEmpty('image') && (
          <div className="explorer-empty">No images found</div>
        )}
      </section>

      <section className="explorer-section">
        <div className="explorer-section-title">AUDIO</div>
        {audio.map((a) => (
          <div
            key={a.path}
            className="explorer-item"
            title={a.path}
            draggable
            onDragStart={(e) => {
              e.dataTransfer.setData(
                'application/x-caesura-asset',
                JSON.stringify({ path: a.path, type: a.type }),
              )
              e.dataTransfer.effectAllowed = 'copy'
            }}
          >
            <span className="explorer-file-icon">🎵</span>
            <span className="explorer-item-name">{a.name}</span>
            <button
              className="explorer-audio-btn"
              title="Play (no audio service endpoint)"
              onClick={(e) => {
                e.stopPropagation()
                previewAudio(a.path)
              }}
            >
              ▶
            </button>
          </div>
        ))}
        {audio.length === 0 && showEmpty('audio') && (
          <div className="explorer-empty">No audio found</div>
        )}
      </section>

      {error && <div className="panel-error">{error}</div>}
    </div>
  )
}

/**
 * Attempt to preview an audio asset via the browser Audio API. The engine
 * has no media streaming endpoint, so this is a degraded best-effort: on
 * failure (or when no resolvable source exists) we surface an
 * "unavailable" hint instead of throwing.
 */
function previewAudio(path: string): void {
  try {
    const el = new Audio(path)
    void el.play().catch(() => {
      showAudioUnavailable()
    })
  } catch {
    showAudioUnavailable()
  }
}

function showAudioUnavailable(): void {
  // The engine exposes no media service endpoint, so playback is expected
  // to fail for relative asset paths. Surface a transient hint on the pane.
  const pane = document.querySelector('.sidebar-pane')
  const hint = document.createElement('div')
  hint.className = 'panel-msg'
  hint.textContent = '不可用 — 引擎无媒体服务端点'
  pane?.appendChild(hint)
  window.setTimeout(() => hint.remove(), 2500)
}
