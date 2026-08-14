import { describe, it, expect } from 'vitest'
import { luaString, luaValue } from './luaString'

describe('luaString (long-bracket escaping)', () => {
  it('wraps plain text at level 1', () => {
    expect(luaString('hello')).toBe('[=[hello]=]')
  })

  it('plain content with no equals uses level 1', () => {
    expect(luaString('a b c')).toBe('[=[a b c]=]')
  })

  it('content with equals runs escalates the level', () => {
    expect(luaString('a = b')).toBe('[==[a = b]==]')
    expect(luaString('x == y')).toBe('[===[x == y]===]')
  })

  it('terminator-shaped content is escaped (code-injection guard)', () => {
    // The level-1 terminator ]=] inside content must not terminate early.
    const payload = ']=] print(1) --'
    const out = luaString(payload)
    expect(out).toBe('[==[\u005d=\u005d print(1) --]==]') // contains "]=]"
    // The payload must survive a round-trip: the closing bracket is level 2.
    expect(out.endsWith(']==]')).toBe(true)
    // No occurrence of the actual terminator inside the body at its level.
    expect(out.slice(3, -4).includes(']==]')).toBe(false)
  })

  it('multi-byte and newline content is preserved verbatim', () => {
    const s = '中文\nline2\u0000end'
    const out = luaString(s)
    expect(out).toContain('中文')
    expect(out).toContain('line2')
  })

  it('empty string', () => {
    expect(luaString('')).toBe('[=[]=]')
  })

  it('content with many consecutive equals uses matching level', () => {
    const payload = 'a' + '='.repeat(10) + 'b'
    const out = luaString(payload)
    // Level = 11 (10-run + 1); opening is [ + 11= + [.
    const open = '[' + '='.repeat(11) + '['
    const close = ']' + '='.repeat(11) + ']'
    expect(out).toBe(open + payload + close)
    // The level-10 run inside is NOT a terminator at level 11.
    expect(out.includes(']' + '='.repeat(10) + ']')).toBe(false)
  })
})

describe('luaValue (JSON → Lua literal)', () => {
  it('scalars map to Lua types', () => {
    expect(luaValue(null)).toBe('nil')
    expect(luaValue(undefined)).toBe('nil')
    expect(luaValue(true)).toBe('true')
    expect(luaValue(false)).toBe('false')
    expect(luaValue(42)).toBe('42')
    expect(luaValue(1.5)).toBe('1.5')
  })

  it('non-finite numbers become nil', () => {
    expect(luaValue(Infinity)).toBe('nil')
    expect(luaValue(NaN)).toBe('nil')
  })

  it('strings are quoted and escaped', () => {
    expect(luaValue('hi')).toBe('"hi"')
    expect(luaValue('a"b')).toBe('"a\\"b"')
    expect(luaValue('line\nbreak')).toBe('"line\\nbreak"')
  })

  it('arrays become table constructors', () => {
    expect(luaValue([1, 2, 3])).toBe('{1,2,3}')
    expect(luaValue(['a', 'b'])).toBe('{"a","b"}')
    expect(luaValue([])).toBe('{}')
  })

  it('nested arrays/objects convert recursively', () => {
    expect(luaValue({ a: [1, { b: 2 }] })).toBe('{["a"]={1,{["b"]=2}}}')
  })

  it('object keys are quoted table keys', () => {
    expect(luaValue({ key: 'val' })).toBe('{["key"]="val"}')
  })
})