import type { AssetEntry } from './rpc'

/** The type filter values the Asset Browser exposes. */
export type AssetTypeFilter = 'all' | 'image' | 'audio' | 'script'

/**
 * Filter an asset list down to a single coarse type. `'all'` returns every
 * asset unchanged (pure passthrough reference). Assets whose `type` is a
 * known engine value (image/audio/script) are matched exactly; anything else
 * is dropped for a specific filter but kept under `'all'`.
 */
export function filterByType<T extends AssetEntry>(
  assets: readonly T[],
  selectedType: AssetTypeFilter,
): T[] {
  if (selectedType === 'all') return assets.slice()
  return assets.filter((a) => a.type === selectedType)
}
