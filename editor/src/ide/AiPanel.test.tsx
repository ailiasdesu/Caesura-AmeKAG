// @vitest-environment jsdom
import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { AiPanel } from './AiPanel'
import type { EngineClient } from '../lib/rpc'
import { useEditor } from '../store'
import { AI_CHAT_KEY, clearChat, loadChat, saveChat } from '../lib/chatHistory'
import { splitCodeBlocks } from '../lib/codeBlocks'
import { withTimeout } from '../lib/promiseUtil'

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

/** Extract the only Lua code string sent to /api/eval on the [0] mock call. */
const lastCode = (client: Client): string =>
  (client.evalRaw as ReturnType<typeof vi.fn>).mock.calls[0][0] as string

beforeEach(() => {
  cleanup()
  localStorage.clear()
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'ai',
    engineConnected: true,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
    editorSelection: null,
  })
})

describe('AiPanel (component) — writer / dev assist', () => {
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
    const code = lastCode(client)
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
    const code = lastCode(client)
    expect(code).toContain('continue_scene')
    expect(code).toContain('room.png')
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
    const code = lastCode(client)
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
    const client = makeClient(async () => JSON.stringify([{ text: '[]', error: '' }]))
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

describe('AiPanel — Ask query flow (item 1)', () => {
  it('sends an ask through kag.aidev and renders the full reply', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: 'Use [ch] for dialogue.', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    const ta = screen.getByLabelText('AI prompt')
    fireEvent.change(ta, { target: { value: 'how do I write dialogue?' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('Use [ch] for dialogue.')
    const code = lastCode(client)
    expect(code).toContain("require('kag.aidev')")
    expect(code).toContain("ad.json('ask'")
    expect(code).toContain('how do I write dialogue?')
    // user row + assistant row in transcript
    expect(screen.getAllByText('user').length).toBeGreaterThan(0)
  })

  it('renders fenced code blocks as <pre> panels in the reply', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: 'Try:\n```ks\n[ch text="X"]\n```', error: '' }]),
    )
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'snippet' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    const code = await screen.findByTestId('ai-code-block')
    expect(code.tagName).toBe('PRE')
    expect(code.textContent).toContain('[ch text="X"]')
  })

  it('degrades to an error row when the engine returns an AiReply error', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: '', error: 'ollama not reachable' }]),
    )
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'hi' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('AI error: ollama not reachable')
  })

  it('degrades to an error row when evalRaw rejects (engine closed)', async () => {
    const client = makeClient(async () => {
      throw new Error('engine refused connection')
    })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'hi' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    const errs = await screen.findAllByText('engine refused connection')
    expect(errs.length).toBeGreaterThan(0)
    // Ask is not permanently stuck busy: typing a new prompt re-enables it
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'again' } })
    const askBtn = screen.getByRole('button', { name: 'Ask' }) as HTMLButtonElement
    expect(askBtn.disabled).toBe(false)
  })

  it('keeps the Ask button disabled for an empty prompt', () => {
    const client = makeClient(async () => '[]')
    render(<AiPanel client={client as unknown as EngineClient} />)
    const askBtn = screen.getByRole('button', { name: 'Ask' }) as HTMLButtonElement
    expect(askBtn.disabled).toBe(true)
    expect(client.evalRaw).not.toHaveBeenCalled()
  })
})

describe('AiPanel — context injection (item 2)', () => {
  it('injects the active doc tail into the query when enabled', async () => {
    const client = makeClient(async () => JSON.stringify([{ text: 'ok', error: '' }]))
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByTestId('include-doc'))
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'review' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('ok')
    expect(lastCode(client)).toContain('room.png')
    expect(lastCode(client)).toContain('ACTIVE DOCUMENT')
  })

  it('does not inject doc tail when the checkbox is off', async () => {
    const client = makeClient(async () => JSON.stringify([{ text: 'ok', error: '' }]))
    useEditor.setState({ docs: [ACTIVE_DOC], activePath: ACTIVE_DOC.path })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'review' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('ok')
    expect(lastCode(client)).not.toContain('room.png')
  })

  it('injects the selected text when enabled and matching the active doc', async () => {
    const client = makeClient(async () => JSON.stringify([{ text: 'ok', error: '' }]))
    useEditor.setState({
      docs: [ACTIVE_DOC],
      activePath: ACTIVE_DOC.path,
      editorSelection: { path: ACTIVE_DOC.path, text: 'my highlighted line' },
    })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByTestId('include-selection'))
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'fix' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('ok')
    expect(lastCode(client)).toContain('my highlighted line')
    expect(lastCode(client)).toContain('SELECTION')
  })

  it('shows a degraded hint and disables doc injection when no doc is open', async () => {
    const client = makeClient(async () => '[]')
    render(<AiPanel client={client as unknown as EngineClient} />)
    expect(screen.getByTestId('no-doc-hint')).toBeTruthy()
    expect((screen.getByTestId('include-doc') as HTMLInputElement).disabled).toBe(true)
  })
})

describe('AiPanel — history persistence (item 3)', () => {
  it('restores the transcript when the panel is reopened', async () => {
    const client = makeClient(async () =>
      JSON.stringify([{ text: 'persisted reply', error: '' }]),
    )
    const view = render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'remember me' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('persisted reply')
    expect(screen.getByText('remember me')).toBeTruthy()

    view.unmount()
    render(<AiPanel client={client as unknown as EngineClient} />)
    await screen.findByText('persisted reply')
    expect(screen.getByText('remember me')).toBeTruthy()
  })

  it('clears the transcript and storage via the Clear button', async () => {
    const client = makeClient(async () => JSON.stringify([{ text: 'hi', error: '' }]))
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'hello' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('hi')
    expect(localStorage.getItem(AI_CHAT_KEY)).not.toBeNull()

    fireEvent.click(screen.getByText('Clear'))
    expect(screen.getByTestId('ai-empty')).toBeTruthy()
    // persisted as an empty list (still recoverable / harmless)
    expect(loadChat()).toEqual([])
  })
})

describe('AiPanel — engine state gating (item 4)', () => {
  it('disables the whole panel with a banner when the engine is disconnected', async () => {
    useEditor.setState({ engineConnected: false })
    const client = makeClient(async () => '[]')
    render(<AiPanel client={client as unknown as EngineClient} />)
    expect(screen.getByTestId('ai-disconnected')).toBeTruthy()
    expect(
      (screen.getByText('Generate Dialogue').closest('button') as HTMLButtonElement).disabled,
    ).toBe(true)
    expect(
      (screen.getByRole('button', { name: 'Ask' }) as HTMLButtonElement).disabled,
    ).toBe(true)
    expect(screen.queryByTestId('ai-paused')).toBeNull()
  })

  it('shows a pause note but keeps queries enabled when paused', async () => {
    useEditor.setState({ enginePaused: true })
    const client = makeClient(async () => JSON.stringify([{ text: 'still works', error: '' }]))
    render(<AiPanel client={client as unknown as EngineClient} />)
    expect(screen.getByTestId('ai-paused')).toBeTruthy()
    expect(screen.queryByTestId('ai-disconnected')).toBeNull()
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'still?' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('still works')
  })
})

describe('AiPanel — RPC call shape (item 5)', () => {
  it('builds the ask payload with doc path + use flags for /api/eval', async () => {
    const client = makeClient(async () => JSON.stringify([{ text: 'ok', error: '' }]))
    useEditor.setState({
      docs: [ACTIVE_DOC],
      activePath: ACTIVE_DOC.path,
      editorSelection: { path: ACTIVE_DOC.path, text: 'sel' },
    })
    render(<AiPanel client={client as unknown as EngineClient} />)
    fireEvent.click(screen.getByTestId('include-doc'))
    fireEvent.click(screen.getByTestId('include-selection'))
    fireEvent.change(screen.getByLabelText('AI prompt'), { target: { value: 'query' } })
    fireEvent.click(screen.getByRole('button', { name: 'Ask' }))
    await screen.findByText('ok')
    const code = lastCode(client)
    expect(code).toContain('ad.json')
    expect(code).toContain("'ask'")
    expect(code).toContain('assets/script/room.ks')
    expect(code).toContain('["include_doc"]=true')
    expect(code).toContain('["use_selection"]=true')
  })
})

describe('lib helpers (splitCodeBlocks, chatHistory, withTimeout)', () => {
  it('splitCodeBlocks separates prose from fences (and handles unclosed)', () => {
    const blocks = splitCodeBlocks('head\n```ks\n[ch]\n```\ntail')
    expect(blocks.map((b) => b.kind)).toEqual(['text', 'code', 'text'])
    expect(blocks[1].lang).toBe('ks')
    expect(blocks[1].content).toContain('[ch]')
    const noFence = splitCodeBlocks('just prose')
    expect(noFence).toEqual([{ kind: 'text', content: 'just prose' }])
  })

  it('chatHistory round-trips and tolerates corrupt storage', () => {
    const msgs = [
      { id: 'a', role: 'user' as const, text: 'hi', time: 1 },
      { id: 'b', role: 'assistant' as const, text: 'hello', time: 2 },
    ]
    saveChat(msgs, '__t__')
    expect(loadChat('__t__')).toEqual(msgs)
    localStorage.setItem('__t__', '{corrupt')
    expect(loadChat('__t__')).toEqual([])
    localStorage.setItem('__t__', 'null')
    expect(loadChat('__t__')).toEqual([])
    clearChat('__t__')
    expect(loadChat('__t__')).toEqual([])
  })

  it('withTimeout rejects slow work and resolves fast work', async () => {
    await expect(
      withTimeout(
        new Promise<string>(() => {}),
        5,
        'timed out',
      ),
    ).rejects.toThrow('timed out')
    await expect(
      withTimeout(Promise.resolve('done'), 200, 'should not fire'),
    ).resolves.toBe('done')
  })
})