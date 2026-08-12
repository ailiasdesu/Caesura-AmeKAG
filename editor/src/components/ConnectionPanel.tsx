import { useState } from 'react'
import type { EngineClient } from '../lib/rpc'
import type { ConnState } from '../App'

interface Props {
  client: EngineClient
  state: ConnState
  error: string
  onState: (s: ConnState) => void
  onError: (e: string) => void
}

export function ConnectionPanel({ client, state, error, onState, onError }: Props) {
  const [token, setToken] = useState('')
  const [base, setBase] = useState('/api')

  const connect = async () => {
    client.setToken(token.trim())
    client.setBase(base.trim() || '/api')
    onState('connecting')
    try {
      const st = await client.status()
      onState('connected')
      onError(st.lua ? '' : 'Engine connected but Lua VM is not ready')
    } catch (e) {
      onState('error')
      onError(e instanceof Error ? e.message : String(e))
    }
  }

  return (
    <div className="conn-panel">
      <span className={`conn-dot conn-${state}`} title={state} />
      <input
        className="conn-base"
        value={base}
        onChange={(e) => setBase(e.target.value)}
        placeholder="/api"
        title="Engine API base path (dev proxy or same-origin)"
      />
      <input
        className="conn-token"
        type="password"
        value={token}
        onChange={(e) => setToken(e.target.value)}
        placeholder="CAESURA_EDITOR_TOKEN (optional)"
        title="Bearer token matching the engine's CAESURA_EDITOR_TOKEN"
      />
      <button onClick={() => void connect()}>
        {state === 'connected' ? 'Reconnect' : 'Connect'}
      </button>
      {state === 'error' && error && <span className="conn-error">{error}</span>}
    </div>
  )
}
