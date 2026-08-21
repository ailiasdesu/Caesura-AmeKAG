// Sprint 3b: parse the payload the ExplorerView drags out onto the Editor.
// Pure and unit-testable - kept OUT of EditorArea.tsx so importing it does
// not pull in Monaco (which needs a DOM to import).
export function parseAssetDrop(
  raw: string | null | undefined,
): { path: string; type: string } | null {
  if (!raw) return null
  try {
    const v = JSON.parse(raw) as { path?: unknown; type?: unknown }
    if (typeof v.path !== 'string' || typeof v.type !== 'string') return null
    return { path: v.path, type: v.type }
  } catch {
    return null
  }
}
