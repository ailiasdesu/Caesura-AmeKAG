// @vitest-environment jsdom
import { describe, it, expect, vi, afterEach } from 'vitest'
import { render, screen, cleanup, waitFor } from '@testing-library/react'
import { OutputPanel } from './OutputPanel'
import type { EngineClient, LogEntry } from '../lib/rpc'

type Client = Pick<EngineClient, 'logs'>
const makeClient = (logs: () => Promise<LogEntry[]>): Client => ({
  logs: vi.fn(logs),
})

const ENTRIES: LogEntry[] = [
  { level: 'info', message: 'engine started', time: '00:00:01' },
  { level: 'warn', message: 'texture budget low', time: '00:00:02' },
  { level: 'error', message: 'bgfx shader compile failed', time: '00:00:03' },
]

afterEach(() => cleanup())

describe('OutputPanel (component)', () => {
  it('shows the empty hint before any log arrives', async () => {
    const client = makeClient(async () => [])
    render(<OutputPanel client={client as unknown as EngineClient} />)
    expect(screen.getByText('No log entries yet')).toBeTruthy()
  })

  it('renders log lines with level classes and time', async () => {
    const client = makeClient(async () => ENTRIES)
    render(<OutputPanel client={client as unknown as EngineClient} />)
    await screen.findByText('engine started')
    expect(screen.getByText('texture budget low')).toBeTruthy()
    expect(screen.getByText('bgfx shader compile failed')).toBeTruthy()
    expect(screen.getByText('00:00:03')).toBeTruthy()
    const lines = document.querySelectorAll('.log-line')
    expect(lines[0].className).toContain('log-info')
    expect(lines[1].className).toContain('log-warn')
    expect(lines[2].className).toContain('log-error')
    expect(client.logs).toHaveBeenCalled()
  })

  it('caps the visible log at the last 500 entries', async () => {
    const many: LogEntry[] = Array.from({ length: 620 }, (_, i) => ({
      level: 'info',
      message: 'entry ' + i,
      time: 't',
    }))
    const client = makeClient(async () => many)
    render(<OutputPanel client={client as unknown as EngineClient} />)
    await screen.findByText('entry 619')
    const rendered = document.querySelectorAll('.log-line')
    expect(rendered).toHaveLength(500)
    expect(screen.queryByText('entry 0')).toBeNull()
    expect(screen.getByText('entry 120')).toBeTruthy() // first kept = 620-500
  })

  it('survives engine unreachable errors (retry next tick)', async () => {
    const client = makeClient(async () => {
      throw new Error('fetch failed')
    })
    render(<OutputPanel client={client as unknown as EngineClient} />)
    // no crash; empty hint stays
    await waitFor(() => expect(client.logs).toHaveBeenCalled())
    expect(screen.getByText('No log entries yet')).toBeTruthy()
  })
})
