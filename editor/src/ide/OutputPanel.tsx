import { useEffect, useState } from 'react'
import type { EngineClient, LogEntry } from '../lib/rpc'

interface Props {
  client: EngineClient
}

export function OutputPanel({ client }: Props) {
  const [logs, setLogs] = useState<LogEntry[]>([])

  useEffect(() => {
    let alive = true
    let timer: ReturnType<typeof setTimeout> | undefined

    const poll = async () => {
      try {
        const entries = await client.logs()
        if (alive) setLogs(entries.slice(-500))
      } catch {
        /* engine unreachable — retry next tick */
      }
      if (alive) timer = setTimeout(poll, 2000)
    }
    void poll()
    return () => {
      alive = false
      if (timer) clearTimeout(timer)
    }
  }, [client])

  return (
    <div className="output-panel">
      <div className="panel-title">Output · engine logs</div>
      <div className="log-list">
        {logs.length === 0 && <div className="log-empty">No log entries yet</div>}
        {logs.map((l, i) => (
          <div key={i} className={`log-line log-${l.level}`}>
            <span className="log-time">{l.time}</span>
            <span className="log-level">{l.level}</span>
            <span className="log-msg">{l.message}</span>
          </div>
        ))}
      </div>
    </div>
  )
}
