import { useState, useEffect, useCallback } from 'react'

const API = 'http://127.0.0.1:9876'

export default function App() {
  const [assets, setAssets] = useState([])
  const [script, setScript] = useState(`-- Write your KAG scene script here
function scene_start()
    KAG.clear_screen()
    KAG.show_image("bg", "assets/bg/classroom.png", 0, 0)
    KAG.play_bgm("assets/bgm/morning.ogg", 1.0)
    KAG.show_text("Welcome to my visual novel!")
    KAG.wait_click()
end

scene_start()
`)
  const [logs, setLogs] = useState([])
  const [status, setStatus] = useState('disconnected')
  const [activePanel, setActivePanel] = useState('editor')

  // Poll engine status
  useEffect(() => {
    const ping = async () => {
      try {
        const r = await fetch(API + '/api/ping')
        const j = await r.json()
        setStatus(j.status === 'ok' ? 'connected' : 'error')
      } catch {
        setStatus('disconnected')
      }
    }
    ping()
    const iv = setInterval(ping, 3000)
    return () => clearInterval(iv)
  }, [])

  // Poll logs
  useEffect(() => {
    const poll = async () => {
      try {
        const r = await fetch(API + '/api/logs')
        const j = await r.json()
        setLogs(j)
      } catch {}
    }
    poll()
    const iv = setInterval(poll, 2000)
    return () => clearInterval(iv)
  }, [])

  // Load assets
  const loadAssets = useCallback(async () => {
    try {
      const r = await fetch(API + '/api/assets')
      const j = await r.json()
      setAssets(j)
    } catch {}
  }, [])

  useEffect(() => { if (status === 'connected') loadAssets() }, [status, loadAssets])

  // Run script
  const runScript = async () => {
    try {
      await fetch(API + '/api/run', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: script
      })
    } catch {}
  }

  // Stop
  const stopScript = async () => {
    try {
      await fetch(API + '/api/stop', { method: 'POST' })
    } catch {}
  }

  // Insert asset path into script
  const insertAsset = (path) => {
    setScript(s => s + '\n-- ' + path)
  }

  const statusColor = status === 'connected' ? '#4f8' : status === 'error' ? '#f84' : '#f44'

  return (
    <div style={{ display: 'flex', width: '100%', height: '100vh' }}>
      {/* Left: Asset Browser */}
      <div style={panelStyle(activePanel === 'assets' ? '280px' : '48px')}>
        <div style={tabStyle} onClick={() => setActivePanel(p => p === 'assets' ? 'editor' : 'assets')}>
          {activePanel === 'assets' ? '\u25C0 \u7D20\u6750' : '\u25B6'}
        </div>
        {activePanel === 'assets' && (
          <div style={{ flex: 1, overflow: 'auto', padding: 8 }}>
            <div style={sectionTitle}>Images</div>
            {assets.filter(a => a.type === 'image').map(a => (
              <div key={a.path} style={assetItem} onClick={() => insertAsset(a.path)}>
                {a.name}
              </div>
            ))}
            <div style={sectionTitle}>Audio</div>
            {assets.filter(a => a.type === 'audio').map(a => (
              <div key={a.path} style={assetItem} onClick={() => insertAsset(a.path)}>
                {a.name}
              </div>
            ))}
            <div style={sectionTitle}>Scripts</div>
            {assets.filter(a => a.type === 'script').map(a => (
              <div key={a.path} style={assetItem} onClick={() => insertAsset(a.path)}>
                {a.name}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Center: Script Editor */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column' }}>
        <div style={toolbarStyle}>
          <span style={{ fontSize: 13, fontWeight: 600, color: '#c0c0d0' }}>
            Caesura (AmeKAG)
          </span>
          <span style={{ fontSize: 11, padding: '2px 8px', borderRadius: 4, background: statusColor + '22', color: statusColor }}>
            {status}
          </span>
          <div style={{ flex: 1 }} />
          <button style={btnStyle('#4a4')} onClick={runScript}>\u25B6 Run</button>
          <button style={btnStyle('#a44')} onClick={stopScript}>\u25A0 Stop</button>
        </div>
        <textarea
          style={editorStyle}
          value={script}
          onChange={e => setScript(e.target.value)}
          spellCheck={false}
        />
      </div>

      {/* Right: Log Panel */}
      <div style={panelStyle(activePanel === 'logs' ? '300px' : '48px')}>
        <div style={tabStyle} onClick={() => setActivePanel(p => p === 'logs' ? 'editor' : 'logs')}>
          {activePanel === 'logs' ? '\u25B6 \u65E5\u5FD7' : '\u25C0'}
        </div>
        {activePanel === 'logs' && (
          <div style={{ flex: 1, overflow: 'auto', padding: 8, fontFamily: 'monospace', fontSize: 11 }}>
            {logs.map((l, i) => (
              <div key={i} style={{ padding: '1px 0', color: l.level === 'error' ? '#f66' : l.level === 'warn' ? '#fa0' : '#aaa' }}>
                <span style={{ color: '#555' }}>{l.time}</span> {l.message}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  )
}

const panelStyle = (width) => ({
  display: 'flex', flexDirection: 'column',
  borderRight: '1px solid #1a1a24',
  background: '#12121a',
  transition: 'width 0.2s',
  width, minWidth: 48, overflow: 'hidden'
})

const tabStyle = {
  padding: '12px 14px', cursor: 'pointer',
  fontSize: 12, fontWeight: 600,
  color: '#888', userSelect: 'none',
  borderBottom: '1px solid #1a1a24',
  writingMode: 'horizontal-tb', whiteSpace: 'nowrap'
}

const toolbarStyle = {
  display: 'flex', alignItems: 'center', gap: 8,
  padding: '8px 12px', borderBottom: '1px solid #1a1a24',
  background: '#14141e'
}

const editorStyle = {
  flex: 1, background: '#0d0d14', color: '#c8c8d8',
  border: 'none', padding: 16, fontFamily: 'monospace',
  fontSize: 13, lineHeight: 1.6, resize: 'none', outline: 'none',
  tabSize: 2
}

const sectionTitle = {
  fontSize: 10, fontWeight: 700, color: '#555',
  textTransform: 'uppercase', letterSpacing: 1,
  padding: '8px 4px 4px', marginTop: 4
}

const assetItem = {
  padding: '4px 8px', fontSize: 12, cursor: 'pointer',
  borderRadius: 3, color: '#aaa',
  ':hover': { background: '#1a1a28' }
}

const btnStyle = (bg) => ({
  padding: '4px 12px', border: 'none', borderRadius: 4,
  background: bg, color: '#fff', fontSize: 12,
  fontWeight: 600, cursor: 'pointer'
})