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

describe('lspCall (Lua bridge code generation)', () => {
  it('builds a kag.lsp.json call for the method', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', '[bg stor')
    expect(calls).toHaveLength(1)
    expect(calls[0]).toContain("local lsp = require('kag.lsp')")
    expect(calls[0]).toContain("lsp.json('completion'")
    expect(calls[0]).toContain('[=[')
  })

  it('escapes string args with luaString long brackets', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'hover', 'bg', 'storage')
    expect(calls[0]).toContain("[=[bg]=]")
    expect(calls[0]).toContain("[=[storage]=]")
  })

  it('terminator-shaped content escalates the bracket level', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'completion', ']=] print(1)')
    // Level-1 terminator content must not break out of the string.
    expect(calls[0]).toContain('[==[')
  })

  it('non-string args are converted via luaValue', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'diag', 'line', 5, 3)
    expect(calls[0]).toContain('5, 3')
  })

  it('no args omits the comma section', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'noargs')
    expect(calls[0]).toBe("local lsp = require('kag.lsp'); return lsp.json('noargs')")
  })

  it('multi-arg calls join with comma-space', async () => {
    const { client, calls } = mockClient()
    await lspCall(client, 'm', 'a', 'b', 'c')
    expect(calls[0]).toContain("lsp.json('m', [=[a]=], [=[b]=], [=[c]=])")
  })
})
