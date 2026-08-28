// t45: store-level periodic engine-connection heartbeat.
//
// Legacy wiring only pinged ONCE at App mount (App.tsx mount effect) plus the
// explicit Connect button and DebugView's refresh path — so after the engine
// exits, every panel badge (incl. the Build Manager RUN block) stayed
// 'connected' until the next manual action. This hook polls client.ping()
// (GET /api/ping — the existing channel; no new protocol) every intervalMs
// and publishes ONLY engineConnected to the store: success -> true, failure
// -> false. It never writes engineScene/enginePaused, so DebugView's own
// refresh path and the ConnectionPanel flows keep working unchanged.
//
// Lifecycle: the returned cleanup clears the interval; a StrictMode/HMR
// double mount therefore never starts a second timer (mount -> cleanup ->
// mount). The first beat runs immediately so the badges reflect reality the
// moment the app opens.
import { useEffect, useRef } from 'react'
import type { EngineClient } from './rpc'
import { useEditor } from '../store'

export const HEARTBEAT_INTERVAL_MS = 7000

export function useEngineHeartbeat(
  client: EngineClient | null,
  intervalMs: number = HEARTBEAT_INTERVAL_MS,
): void {
  const clientRef = useRef<EngineClient | null>(client)
  clientRef.current = client
  useEffect(() => {
    if (!client) return
    let stopped = false
    let timer: ReturnType<typeof setInterval> | null = null
    const beat = async () => {
      const c = clientRef.current
      if (!c || stopped) return
      const connected = await c.ping().then(() => true).catch(() => false)
      if (stopped) return
      const state = useEditor.getState()
      // Publish only on change: setEngine({...}) triggers no listener work
      // when the value is already correct.
      if (state.engineConnected !== connected) {
        state.setEngine({ engineConnected: connected })
      }
    }
    void beat()
    timer = setInterval(() => {
      void beat()
    }, intervalMs)
    return () => {
      stopped = true
      if (timer !== null) clearInterval(timer)
    }
  }, [client, intervalMs])
}
