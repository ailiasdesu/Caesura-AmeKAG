// t60: extract the t54 conn-state epoch guard into a pure, unit-testable
// helper. Component-level red-green proved unreachable: in the jsdom/RTL
// harness the mount effect's rejected startup ping does not reach the
// connection-state write the DOM assertions observe (removing the guard
// from App.tsx left App.connGuard.test.tsx green in every experiment), so
// this pure guard is the regression LOCK; App uses it and the unit test
// flips red when the staleness check is weakened.

export interface ConnEpochGuard {
  /** Epoch value at the moment a write sequence STARTS. */
  stamp(): number
  /** Invalidate every earlier stamp (manual Connect = new authority). */
  bump(): void
  /** True when a stamped write is no longer the authority. */
  isStale(stamp: number): boolean
}

export function createConnEpochGuard(): ConnEpochGuard {
  let epoch = 0
  return {
    stamp: () => epoch,
    bump: () => { epoch++ },
    isStale: (s) => s !== epoch,
  }
}
