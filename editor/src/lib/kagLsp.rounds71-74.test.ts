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
// ---------------------------------------------------------------------------
// Round 75: LSP-side [sel] param completion via the alias mechanism
// (ALIAS_PARAM_CMDS = { sel = "button" } in scripts/kag/lsp.lua). The bridge
// forwards the full tag line so the Lua server can do prefix-filtered param
// completion from the aliased [button] contract (text/target/cond/caption/x).
// These tests pin the CLIENT side of that contract; the Lua-side behaviour is
// covered by tests/scripts/test_lsp.lua.
// ---------------------------------------------------------------------------

describe('round-75 [sel] param completion (alias ctrl)', () => {
  it('sends "[sel " so the alias resolves to the [button] contract params', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[sel ')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("lsp.json('completion'")
    // the full tag-with-trailing-space reaches the server: nameComplete is
    // true for "[sel " and Lua then completes params from button's contract.
    // luaString wraps the content (which itself starts with '[') as
    // [=[<content>]=] => '[=[' + '[sel ' + ']=]' => '[=[[sel ]=]'.
    expect(calls[0]).toContain('[=[[sel ]=]')
  })

  it('sends "[sel " with a cursor column so Lua surfaces the param prefix', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[sel ', 6)
    expect(calls[0]).toContain('[=[[sel ]=]')
    expect(calls[0]).toMatch(/, 6\)/)
  })

  it('surfaces the [button]-aliased param set from the returned JSON', async () => {
    // Mirror what lsp.lua's ALIAS_PARAM_CMDS actually returns: the [sel]
    // completion resolves to the [button] contract (text/target/cond/caption/
    // x). The bridge must NOT filter this - it carries Lua's answer through.
    const calls: string[] = []
    const client = {
      evalRaw: vi.fn(async (code: string) => {
        calls.push(code)
        // lsp.json array of {label, kind, detail, insertText} for button params
        const items = [
          ['text', 'string = '], ['caption', 'string'],
          ['target', 'string'], ['cond', 'string'], ['x', 'string'],
        ].map(([label, detail]) =>
          '{"label":"' + label + '","kind":5,"detail":"' + detail + '"}')
        return '[' + items.join(',') + ']'
      }),
    } as unknown as EngineClient
    const json = await lspCall(client, 'completion', '[sel ')
    for (const p of ['text', 'target', 'cond', 'caption', 'x']) {
      expect(json).toContain('"label":"' + p + '"')
    }
  })

  it("forwards the prefix '[sel x' so Lua filters to x= (choice capture)", async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[sel x')
    expect(calls[0]).toContain("lsp.json('completion'")
    // the partial prefix is sent verbatim; the Lua side filters its contract
    // params by the current prefix (only x matches "[sel x")
    expect(calls[0]).toContain('[=[[sel x]=]')
  })
})

describe('round-75 KAG3-style diagnostics (csp left= / add var=)', () => {
  it('forwards [csp left=...] verbatim so the Lua linter sees the param', async () => {
    const { client, calls } = mockClient()
    // KAG3-style csp left= - the Neo-Genesis csp contract declares
    // name/layer/x/y/storage/file/path (no left). The editor bridge must not
    // pre-filter it; the engine's lsp.lua decides validity (it currently
    // reports "unknown param 'left'" for [csp] as a warning).
    await lspCall(client, 'diagnostics', '[csp name="hero" left=0]')
    expect(calls[0]).toContain("lsp.json('diagnostics'")
    expect(calls[0]).toContain('[csp name="hero" left=0]')
  })

  it('forwards [add var=...] verbatim so the Lua linter sees the KAG3 param', async () => {
    const { client, calls } = mockClient()
    // KAG3 [add var=...] vs this engine's Neo-Genesis [add name=...]: the
    // bridge passes the raw tag through and lets lsp.lua judge it (it
    // currently reports "unknown param 'var'" for [add] as a warning).
    await lspCall(client, 'diagnostics', '[add var="f.hp" value=10]')
    expect(calls[0]).toContain("lsp.json('diagnostics'")
    expect(calls[0]).toContain('[add var="f.hp" value=10]')
  })
})

describe('round-75 [button] x= hover', () => {
  it("requests hover for button's x param (choice-capture, type string)", async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'hover', 'button', 'x')
    expect(calls[0]).toContain("lsp.json('hover'")
    expect(calls[0]).toContain('[=[button]=]')
    expect(calls[0]).toContain('[=[x]=]')
  })

  it('surfaces the x= param detail (type string) from the returned JSON', async () => {
    const client = {
      evalRaw: vi.fn(async () => '{"title":"[button]","text":"param `x`  type=string"}'),
    } as unknown as EngineClient
    const json = await lspCall(client, 'hover', 'button', 'x')
    expect(json).toContain('"title":"[button]"')
    expect(json).toContain('type=string')
  })
})


