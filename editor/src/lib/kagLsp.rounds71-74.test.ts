import { describe, it, expect, vi } from 'vitest'
import { lspCall } from './kagLsp'
import { EngineClient } from './rpc'

function mockClient() {
  const calls: string[] = []
  const client = {
    evalRaw: vi.fn(async (code: string) => {
      calls.push(code)
      return '{}'
    }),
  } as unknown as EngineClient
  return { client, calls }
}

// ---------------------------------------------------------------------------
// Rounds 71-74: the editor must forward the right LSP probe for the newly
// added commands (math/character/textspeed/palette/vibrate/notify/preload)
// and the round-74 [button]/[sel] x= choice-capture parameter. These tests
// pin the CLIENT side of the contract: the exact method + escaped payload
// the editor sends to scripts/kag/lsp.lua (whose behaviour is covered by
// tests/scripts/test_lsp.lua). A regression here would silently silence
// completion/hover/diagnostics for those commands.
// ---------------------------------------------------------------------------

describe('round-71/72 command completion probes', () => {
  const commands = [
    ['[add ', '[add '],
    ['[sub ', '[sub '],
    ['[mul ', '[mul '],
    ['[div ', '[div '],
    ['[mod ', '[mod '],
    ['[dec ', '[dec '],
    ['[csp ', '[csp '],
    ['[csd ', '[csd '],
    ['[csl ', '[csl '],
    ['[textspeed ', '[textspeed '],
    ['[cps ', '[cps '],
    ['[palette ', '[palette '],
    ['[vibrate ', '[vibrate '],
    ['[notify ', '[notify '],
    ['[preload ', '[preload '],
  ]

  it.each(commands)('dispatches a completion probe for %s', async (_label, line) => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', line)
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('completion'")
    expect(calls[0]).toContain('[=[')
    expect(calls[0]).toContain(line.slice(1))
  })

  it('sends the cursor column for a ${} expression inside an arithmetic tag', async () => {
    const { client, calls } = mockClient()
    // [add name="f.x" value="hp ${  -> cursor inside the expression span
    await lspCall(client, 'completion', '[add name="f.x" value="hp ${', 30)
    expect(calls[0]).toMatch(/, 30\)/)
    expect(calls[0]).toContain('[==[')
  })
})

describe('round-74 [button]/[sel] x= completion probe', () => {
  it('dispatches completion for [sel x= (choice capture)', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[sel x=')
    expect(calls[0]).toContain("lsp.json('completion'")
    expect(calls[0]).toContain('sel')
  })

  it('dispatches completion for [button x= (choice capture)', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[button x=')
    expect(calls[0]).toContain("lsp.json('completion'")
    expect(calls[0]).toContain('button')
  })

  it('dispatches completion for endselect to keep the choice block closable', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[endselect')
    expect(calls[0]).toContain("lsp.json('completion'")
    expect(calls[0]).toContain('endselect')
  })
})

describe('round-71/72 hover probes', () => {
  it.each([
    ['[add]', 'add'],
    ['[csp]', 'csp'],
    ['[textspeed]', 'textspeed'],
    ['[palette]', 'palette'],
    ['[vibrate]', 'vibrate'],
    ['[notify]', 'notify'],
    ['[preload]', 'preload'],
  ] as const)('dispatches a hover probe for %s', async (_label, cmd) => {
    const { client, calls } = mockClient()
    await lspCall(client, 'hover', cmd)
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('hover'")
    expect(calls[0]).toContain('[=[' + cmd + ']=]')
  })

  it('dispatches a hover probe for the math name param', async () => {
    const { client, calls } = mockClient()
    // math uses param 'name' (not 'var'); the client must forward it
    await lspCall(client, 'hover', 'add', 'name')
    expect(calls[0]).toContain('[=[add]=]')
    expect(calls[0]).toContain('[=[name]=]')
  })
})

describe('round-71/72 diagnostics probe', () => {
  it('sends a whole scene of new commands to diagnostics for linting', async () => {
    const { client, calls } = mockClient()
    const scene = [
      '[add name="f.hp" value=10]',
      '[csp name="hero" x=320 y=240]',
      '[textspeed cps=40]',
      '[palette effect="night"]',
      '[vibrate time=200 intensity=3]',
      '[notify msg="saved"]',
      '[preload type="texture" path="bg/01.png" wait="true"]',
      '[sel x="tf.result"]',
      '[button text="Go" x="tf.go"]',
    ].join('\n')
    await lspCall(client, 'diagnostics', scene)
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('diagnostics'")
    // the whole multi-line scene reaches the server inside ONE long string
    expect(calls[0]).toContain('[add name="f.hp" value=10]')
    expect(calls[0]).toContain('[button text="Go" x="tf.go"]')
  })

  it('forwards math name/value params verbatim (no client-side filtering)', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'diagnostics', '[add name="f.a" value=2]')
    expect(calls[0]).toContain("lsp.json('diagnostics'")
    expect(calls[0]).toContain('[add name="f.a" value=2]')
  })
})

