// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { AiPanel } from './AiPanel'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'

type Client = Pick<EngineClient, 'evalRaw'>
const makeClient = (evalRaw: () => Promise<string>): Client => ({
  evalRaw: vi.fn(evalRaw),
})

const ACTIVE_DOC = {
  path: 'assets/script/room.ks',
  name: 'room.ks',
  language: 'kag',
  content: '*start\n[bg storage="room.png"]\n',
  dirty: false,
}

beforeEach(() => {
  cleanup()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'ai',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
  })
})

describe('AiPanel (component)', () => {
  it('generates dialogue and inserts the reply into the active doc', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '[ch name="Aoi" text="Hello"]', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)

    fireEvent.click(screen.getByText('Generate Dialogue'))
    await screen.findByText('Inserted 1 lines')
    const s = useEditor.getState()
    expect(s.docs[0].content).toContain('[ch name="Aoi" text="Hello"]')
    expect(s.docs[0].dirty).toBe(true)
    // the bridge code goes through kag.aiwriter and luaString escaping
    const code = (client.evalRaw as ReturnType<typeof vi.fn>).mock.calls[0][0] as string
    expect(code).toContain("require('kag.aiwriter')")
    expect(code).toContain('"Aoi, Ryo"')
  })

  it('shows the AI error hint when the reply carries an error', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '', error: 'ollama timeout' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)

    fireEvent.click(screen.getByText('Generate Dialogue'))
    await screen.findByText(/AI error: ollama timeout \(is Ollama running on :11434\?\)/)
    expect(useEditor.getState().docs[0].content).not.toContain('Aoi')
  })

  it('reports eval failures in the message area', async () => {
    const client = makeClient(async () => {
      throw new Error('engine unreachable')
    })
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)

    fireEvent.click(screen.getByText('Generate Dialogue'))
    await screen.findByText('engine unreachable')
  })

  it('disables Continue Scene when no script is open', async () => {
    const client = makeClient(async () => '[]')
    render(<AiPanel client={client as unknown as EngineClient} />)
    const btn = screen.getByText('Continue Scene').closest('button') as HTMLButtonElement
    expect(btn.disabled).toBe(true)
    // empty active list => the guard message is unreachable via click (disabled),
    // but the component's internal guard is exercised by the next test.
    expect(client.evalRaw).not.toHaveBeenCalled()
  })

  it('continues a scene with the tail of the active doc', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '; next beat\n[ch text="More"]', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Continue Scene'))
    await screen.findByText('Appended 2 lines')
    const code = (client.evalRaw as ReturnType<typeof vi.fn>).mock.calls[0][0] as string
    expect(code).toContain('continue_scene')
    expect(code).toContain('room.png') // tail content embedded
  })

  it('requests a scene spec before generating a scene', async () => {
    const client = makeClient(async () => '[]')
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Generate Scene'))
    await screen.findByText('Describe the scene first (e.g. "a rainy classroom confession")')
    expect(client.evalRaw).not.toHaveBeenCalled()
  })

  it('generates a scene from the spec and inserts it', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '; scene skeleton\n*rainy', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByPlaceholderText('a rainy classroom confession with a choice'), {
      target: { value: 'a rainy confession' },
    })
    fireEvent.click(screen.getByText('Generate Scene'))
    await screen.findByText(/Inserted 2 lines/)
    expect(useEditor.getState().docs[0].content).toContain('*rainy')
  })

  it('explains a diagnostic with line prefix parsed from the input', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: 'wiat is not a command', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByPlaceholderText('3: unknown KAG command \'wiat\''), {
      target: { value: '3: unknown KAG command wiat' },
    })
    fireEvent.click(screen.getByText('Explain Diagnostic'))
    await screen.findByText('wiat is not a command')
    const code = (client.evalRaw as ReturnType<typeof vi.fn>).mock.calls[0][0] as string
    expect(code).toContain('explain_diagnostic')
    expect(code).toContain('["line"]=3')
  })

  it('reviews the scene and renders findings as L<line> rows', async () => {
    const client = makeClient(async () =>
      JSON.stringify([
        { text: JSON.stringify([{ line: 2, message: 'unclosed [if]' }]), error: '' },
      ]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Review Scene'))
    await screen.findByText('L2: unclosed [if]')
  })

  it('shows the pass message when review finds no issues', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '[]', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Review Scene'))
    await screen.findByText(/✅ 结构检查通过/)
  })

  it('disables Generate while busy and re-enables after', async () => {
    let release: (v: string) => void = () => {}
    const gate = new Promise<string>((res) => {
      release = res
    })
    const client = makeClient(() => gate)
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByText('Generate Dialogue'))
    const btn = await screen.findByText('Generating…')
    expect((btn as HTMLButtonElement).disabled).toBe(true)
    release(JSON.stringify([{ text: 'ok', error: '' }]))
    await screen.findByText('Inserted 1 lines')
    expect(screen.getByText('Generate Dialogue').closest('button')?.disabled).toBe(false)
  })
})
