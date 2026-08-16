import { useEffect, useRef, useState } from 'react'
import { EngineClient } from './lib/rpc'
import { registerKagLanguage } from './ide/kagLanguage'
import { ActivityBar } from './ide/ActivityBar'
import { StatusBar } from './ide/StatusBar'
import { ExplorerView } from './ide/ExplorerView'
import { SceneTree } from './ide/SceneTree'
import { SceneOutlinePanel } from './ide/SceneOutlinePanel'
import { InspectorView } from './ide/InspectorView'
import { SceneBuilder } from './ide/SceneBuilder'
import { TimelineView } from './ide/TimelineView'
import { DebugView } from './ide/DebugView'
import { VisualView } from './ide/VisualView'
import { AiPanel } from './ide/AiPanel'
import { SettingsPanel } from './ide/SettingsPanel'
import { EditorArea } from './ide/EditorArea'
import { OutputPanel } from './ide/OutputPanel'
import { ConnectionPanel } from './components/ConnectionPanel'
import { useEditor } from './store'
import './styles.css'

export type ConnState = 'disconnected' | 'connecting' | 'connected' | 'error'

registerKagLanguage()

export function App() {
  const clientRef = useRef<EngineClient>(new EngineClient())
  const [conn, setConn] = useState<ConnState>('disconnected')
  const [connError, setConnError] = useState('')
  const sideView = useEditor((s) => s.sideView)
  const setEngine = useEditor((s) => s.setEngine)
  // Layer "settings" entry: the workbench theme is applied via data-theme on
  // the root app node so the CSS palette can react to the persisted choice.
  const theme = useEditor((s) => s.settings.theme)

  useEffect(() => {
    void (async () => {
      setConn('connecting')
      try {
        const st = await clientRef.current.ping()
        void st
        setConn('connected')
        setEngine({ engineConnected: true })
      } catch (e) {
        setConn('error')
        setConnError(e instanceof Error ? e.message : String(e))
        setEngine({ engineConnected: false })
      }
    })()
  }, [setEngine])

  return (
    <div className="app" data-theme={theme}>
      <header className="app-header">
        <h1 className="app-title">Caesura Editor</h1>
        <ConnectionPanel
          client={clientRef.current}
          state={conn}
          error={connError}
          onState={setConn}
          onError={setConnError}
        />
      </header>

      <div className="workbench">
        <ActivityBar />
        <aside className="sidebar">
          {sideView === 'explorer' && (
            <>
              <ExplorerView client={clientRef.current} />
              <SceneTree />
              <SceneOutlinePanel client={clientRef.current} />
              <InspectorView client={clientRef.current} />
              <SceneBuilder />
              <TimelineView />
            </>
          )}
          {sideView === 'debug' && <DebugView client={clientRef.current} />}
          {sideView === 'visual' && <VisualView client={clientRef.current} />}
          {sideView === 'ai' && <AiPanel client={clientRef.current} />}
          {sideView === 'settings' && <SettingsPanel />}
        </aside>

        <main className="editor-col">
          <EditorArea />
          <OutputPanel client={clientRef.current} />
        </main>
      </div>

      <StatusBar />
    </div>
  )
}
