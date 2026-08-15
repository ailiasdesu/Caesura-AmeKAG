// G4 final increment — live engine position polling hook.
//
// Drives a lightweight poll of the RUNNING KAG scene position (scene +
// token_index) by evaluating the position-probe snippet against /api/eval
// (EngineClient.evalRaw). The result is pushed into the editor store's
// engineScene / engineToken fields, which the SceneOutline panel reads to
// highlight the row matching the live execution point.
//
// Lifecycle is guarded so the hook is safe with or without an engine:
//   - When engineConnected is false OR no client is supplied, it sets up no
//     polling at all (no requests, no timers leak).
//   - Each probe rejection / transport error is swallowed (no crash); a
//     transient failure keeps the previous position and polling continues.
//   - The interval is torn down on unmount or when the engine disconnects.

import { useEffect, useRef } from 'react'
import { useEditor } from '../store'
import { buildPositionProbeSnippet, parsePositionProbe } from '../lib/enginePosition'
import type { EngineClient } from '../lib/rpc'

export interface UseEnginePositionOptions {
  /** Engine RPC client; polling only starts when provided. */
  client?: EngineClient
  /** Whether the engine is currently connected. */
  enabled: boolean
  /** Poll interval in ms (default 1500). */
  intervalMs?: number
}

/**
 * Poll the engine for its live position while connected and push it into the
 * store. Returns nothing; the SceneOutline panel reads the store directly.
 * Never throws and never runs when disabled.
 */
export function useEnginePosition({
  client,
  enabled,
  intervalMs = 1500,
}: UseEnginePositionOptions): void {
  const setEngine = useEditor((s) => s.setEngine)
  // Keep the client in a ref so the interval closure never goes stale and a
  // stable client instance does not restart the timer every render.
  const clientRef = useRef<EngineClient | undefined>(client)
  clientRef.current = client

  useEffect(() => {
    // Disconnected or no client: no timer, no requests, no store writes.
    if (!enabled || !clientRef.current) return

    const probe = async () => {
      const c = clientRef.current
      if (!c) return
      try {
        const result = await c.evalRaw(buildPositionProbeSnippet())
        const pos = parsePositionProbe(result)
        if (pos) {
          setEngine({
            engineScene: pos.scene,
            engineToken: pos.token,
          })
        }
      } catch {
        // Transport / engine rejection — keep the previous position; the
        // disconnect detection is the caller's job (engineConnected).
      }
    }

    void probe()
    const t = setInterval(() => void probe(), intervalMs)
    return () => clearInterval(t)
  }, [enabled, intervalMs, setEngine])
}
