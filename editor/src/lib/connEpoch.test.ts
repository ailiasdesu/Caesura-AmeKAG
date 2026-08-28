// t60: unit lock for the t54 conn-state epoch guard (see connEpoch.ts header
// for why the component-level detector cannot lock this).
import { describe, it, expect } from 'vitest'
import { createConnEpochGuard } from './connEpoch'

describe('conn epoch guard (t60 unit lock)', () => {
  it('a fresh stamp is current', () => {
    const g = createConnEpochGuard()
    const s = g.stamp()
    expect(g.isStale(s)).toBe(false)
  })

  it('bump invalidates earlier stamps -- removing this check must go red', () => {
    const g = createConnEpochGuard()
    const s = g.stamp()
    g.bump()
    expect(g.isStale(s)).toBe(true)
  })

  it('a stamp taken after bump stays current', () => {
    const g = createConnEpochGuard()
    g.bump()
    const s2 = g.stamp()
    expect(g.isStale(s2)).toBe(false)
  })

  it('two bumps invalidate the first while the latest stays current', () => {
    const g = createConnEpochGuard()
    const s = g.stamp()
    g.bump()
    g.bump()
    expect(g.isStale(s)).toBe(true)
    expect(g.isStale(g.stamp())).toBe(false)
  })
})
