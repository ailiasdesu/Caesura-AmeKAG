import { useEffect, useRef, useState } from 'react'
import { EngineClient } from './lib/rpc'
import { useEngineHeartbeat } from './lib/engineHeartbeat'
import { createConnEpochGuard } from './lib/connEpoch'
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
import { ProjectManagerView } from './ide/ProjectManagerView'
import { BuildManagerView } from './ide/BuildManagerView'
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
  const setEngineClient = useEditor((s) => s.setEngineClient)
  // Layer "settings" entry: the workbench theme is applied via data-theme on
  // the root app node so the CSS palette can react to the persisted choice.
  const theme = useEditor((s) => s.settings.theme)

  // t45: store-level connection heartbeat (7s; pin only engineConnected).
  useEngineHeartbeat(clientRef.current)

  // t54 conn-state epoch guard: the mount effect's startup ping runs WITHOUT a
  // token (StrictMode double-mount runs it twice). A manual Connect can succeed
  // (status 200) while a stale startup-ping catch is still settling — that late
  // write would downgrade conn/engineConnected back to error/false. Every App
  // conn-state write stamps the epoch at the moment it STARTED; a manual
  // Connect (via bumpConnEpoch) invalidates earlier stamps, so stale writes are
  // dropped. DebugView's refresh and the heartbeat write fresh truth directly
  // and are unaffected. StrictMode-safe: both mount effects stamp the same
  // epoch and are invalidated together.
  // t60: the guard body moved to lib/connEpoch.ts (pure, unit-locked) -- the
  // component-level detector could not distinguish guard presence (see
  // App.connGuard.test.tsx header note); connEpoch.test.ts is the regression
  // lock for the staleness semantics below.
  const connEpochRef = useRef<ReturnType<typeof createConnEpochGuard>>(createConnEpochGuard())
  const bumpConnEpoch = () => {
    connEpochRef.current.bump()
  }

  useEffect(() => {
    setEngineClient(clientRef.current)
    const epoch = connEpochRef.current.stamp()
    void (async () => {
      setConn('connecting')
      try {
        const st = await clientRef.current.ping()
        void st
        if (connEpochRef.current.isStale(epoch)) return
        setConn('connected')
        setEngine({ engineConnected: true })
      } catch (e) {
        if (connEpochRef.current.isStale(epoch)) return
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
          onState={(s) => {
            // A manual Connect result is authoritative: bump the epoch so any
            // still-settling startup ping loses its right to write. (t54)
            if (s === 'connected' || s === 'connecting') bumpConnEpoch()
            setConn(s)
          }}
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
          {sideView === 'project' && <ProjectManagerView client={clientRef.current} />}
          {sideView === 'build' && <BuildManagerView client={clientRef.current} />}
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