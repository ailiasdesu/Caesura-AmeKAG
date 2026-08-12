import { useCallback, useEffect, useState } from 'react'
import type { AssetEntry, EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

interface Props {
  client: EngineClient
}

function langFor(path: string): string {
  return path.toLowerCase().endsWith('.ks') ? 'kag' : 'lua'
}

export function ExplorerView({ client }: Props) {
  const [assets, setAssets] = useState<AssetEntry[]>([])
  const [filter, setFilter] = useState('')
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

  const filtered = filter
    ? assets.filter((a) => a.name.toLowerCase().includes(filter.toLowerCase()))
    : assets

  const scripts = filtered.filter((a) => a.type === 'script')
  const images = filtered.filter((a) => a.type === 'image')
  const audio = filtered.filter((a) => a.type === 'audio')

  const openScript = async (a: AssetEntry) => {
    // Scripts load through the engine eval (sandbox io read) — the editor
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
        {scripts.length === 0 && (
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
            onDoubleClick={() => openImage(a)}
          >
            <span className="explorer-file-icon">🖼</span>
            <span className="explorer-item-name">{a.name}</span>
          </div>
        ))}
        {images.length === 0 && (
          <div className="explorer-empty">No images found</div>
        )}
      </section>

      <section className="explorer-section">
        <div className="explorer-section-title">AUDIO</div>
        {audio.map((a) => (
          <div key={a.path} className="explorer-item" title={a.path}>
            <span className="explorer-file-icon">🎵</span>
            <span className="explorer-item-name">{a.name}</span>
          </div>
        ))}
        {audio.length === 0 && (
          <div className="explorer-empty">No audio found</div>
        )}
      </section>

      {error && <div className="panel-error">{error}</div>}
    </div>
  )
}
