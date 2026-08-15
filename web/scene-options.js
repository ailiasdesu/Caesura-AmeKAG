// scene-options.js — pure helpers for the web player scene picker.
// Turns a story-bundle scene key list into grouped <option>-friendly
// entries, so the picker reflects the bundle instead of a hard-coded list.

const GROUP_DEMO = 'demo'
const GROUP_TUTORIAL = 'tutorial'
const GROUP_OTHER = 'other'

/** Classify a scene key (basename like 'galgame_demo.ks') into a group.
 *  tutorial_*  -> tutorial; *_demo.ks / showcase.ks -> demo; else other. */
export function classifyScene(key) {
  if (typeof key !== 'string') return GROUP_OTHER
  const base = key.split(/[/\\]/).pop() || key
  if (base.startsWith('tutorial_')) return GROUP_TUTORIAL
  if (base.endsWith('_demo.ks') || base === 'showcase.ks' || base === 'galgame.ks') return GROUP_DEMO
  return GROUP_OTHER
}

export const GROUP_LABELS = {
  [GROUP_DEMO]: '演示 (Demo)',
  [GROUP_TUTORIAL]: '教程 (Tutorial)',
  [GROUP_OTHER]: '其他 (Other)',
}

/** Build grouped options from a bundle scenes map (or plain array of keys).
 *  Returns [{ group, label, options: [{ value, label }] }] in a stable order:
 *  demo, tutorial, other, each sorted alphabetically. */
export function buildSceneOptions(scenes) {
  const keys = Array.isArray(scenes)
    ? scenes
    : (scenes && typeof scenes === 'object' ? Object.keys(scenes) : [])
  const groups = { [GROUP_DEMO]: [], [GROUP_TUTORIAL]: [], [GROUP_OTHER]: [] }
  for (const key of keys) groups[classifyScene(key)].push(key)
  const order = [GROUP_DEMO, GROUP_TUTORIAL, GROUP_OTHER]
  const out = []
  for (const g of order) {
    const list = groups[g].sort()
    if (list.length === 0) continue
    out.push({ group: g, label: GROUP_LABELS[g], options: list.map((v) => ({ value: v, label: v })) })
  }
  return out
}
