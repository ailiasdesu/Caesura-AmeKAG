// scene-options.test.js — scene picker grouping logic (round 44, deepened).
// NOTE: this module is a PURE, STATELESS helper that classifies scene keys
// (tutorial_* / *_demo.ks / showcase.ks / galgame.ks / other) and builds
// grouped <option>-friendly entries. It holds no player-settings state
// (resolution / language / auto / skip are NOT part of this module), so the
// deep coverage below targets its real contracts: classification taxonomy,
// input validation, grouping order/default-empty semantics, and purity.
import { describe, it, expect } from 'vitest'
import { classifyScene, buildSceneOptions, GROUP_LABELS } from './scene-options.js'

describe('classifyScene — taxonomy', () => {
  it('groups any basename with the tutorial_ prefix into the tutorial group', () => {
    expect(classifyScene('tutorial_01_hello.ks')).toBe('tutorial')
    expect(classifyScene('tutorial_a_b_c.ks')).toBe('tutorial')
    expect(classifyScene('tutorial_galgame_demo.ks')).toBe('tutorial') // prefix wins over demo
  })

  it('treats tutorial_ as a prefix, so bare "tutorial" is not tutorial', () => {
    expect(classifyScene('tutorial')).toBe('other')
    expect(classifyScene('tutorials.ks')).toBe('other')
  })

  it('groups *_demo.ks, showcase.ks and galgame.ks into the demo group', () => {
    expect(classifyScene('galgame_demo.ks')).toBe('demo')
    expect(classifyScene('full_pipeline_demo.ks')).toBe('demo')
    expect(classifyScene('sma_demo.ks')).toBe('demo')
    expect(classifyScene('showcase.ks')).toBe('demo')
    expect(classifyScene('galgame.ks')).toBe('demo')
  })

  it('requires the exact _demo.ks suffix (mydemo.ks is NOT a demo)', () => {
    expect(classifyScene('mydemo.ks')).toBe('other')
    expect(classifyScene('demo.ks')).toBe('other')
    expect(classifyScene('x_demo.txt')).toBe('other')
  })

  it('classifies by the basename even when nested behind directories', () => {
    expect(classifyScene('demo/tutorial/tutorial_05_branching.ks')).toBe('tutorial')
    expect(classifyScene('scenes/intro/galgame_demo.ks')).toBe('demo')
    expect(classifyScene('nested/showcase.ks')).toBe('demo')
    expect(classifyScene('a/b/c/story.ks')).toBe('other')
  })

  it('handles backslash (Windows-style) and mixed separators', () => {
    expect(classifyScene('demo\\galgame_demo.ks')).toBe('demo')
    expect(classifyScene('tutorial\\tutorial_01.ks')).toBe('tutorial')
    expect(classifyScene('dir1/dir2\\x_demo.ks')).toBe('demo')
  })
})

describe('classifyScene — input validation (non-string falls back to other, never crashes)', () => {
  it('returns other for null / undefined', () => {
    expect(classifyScene(null)).toBe('other')
    expect(classifyScene(undefined)).toBe('other')
  })

  it('returns other for numeric and boolean inputs', () => {
    expect(classifyScene(0)).toBe('other')
    expect(classifyScene(1)).toBe('other')
    expect(classifyScene(Number.NaN)).toBe('other')
    expect(classifyScene(true)).toBe('other')
    expect(classifyScene(false)).toBe('other')
  })

  it('returns other for object and array inputs', () => {
    expect(classifyScene({ a: 1 })).toBe('other')
    expect(classifyScene(['tutorial_01.ks'])).toBe('other')
  })

  it('returns other for empty and separator-only strings', () => {
    expect(classifyScene('')).toBe('other')
    expect(classifyScene('/')).toBe('other')
    expect(classifyScene('\\')).toBe('other')
    expect(classifyScene('demo/')).toBe('other')
  })
})

describe('buildSceneOptions — grouping, order and defaults', () => {
  const bundleScenes = {
    'galgame_demo.ks': {},
    'sma_demo.ks': {},
    'tutorial_02_text.ks': {},
    'tutorial_01_hello.ks': {},
    'showcase.ks': {},
    'story.ks': {},
  }

  it('groups options in demo -> tutorial -> other order, sorted within group', () => {
    const out = buildSceneOptions(bundleScenes)
    expect(out.map((g) => g.group)).toEqual(['demo', 'tutorial', 'other'])
    expect(out[0].options.map((o) => o.value)).toEqual([
      'galgame_demo.ks', 'showcase.ks', 'sma_demo.ks',
    ])
    expect(out[1].options.map((o) => o.value)).toEqual([
      'tutorial_01_hello.ks', 'tutorial_02_text.ks',
    ])
    expect(out[2].options.map((o) => o.value)).toEqual(['story.ks'])
  })

  it('exposes human-readable group labels (and the exported constants are stable)', () => {
    const out = buildSceneOptions(bundleScenes)
    expect(out[0].label).toBe(GROUP_LABELS.demo)
    expect(out[0].label).toContain('演示')
    expect(out[1].label).toBe(GROUP_LABELS.tutorial)
    expect(out[1].label).toContain('教程')
    expect(out[2].label).toBe(GROUP_LABELS.other)
    expect(GROUP_LABELS.other).toContain('其他')
    for (const label of Object.values(GROUP_LABELS)) expect(label.length).toBeGreaterThan(0)
  })

  it('applies taxonomy to object values as well as keys', () => {
    const out = buildSceneOptions({
      'story.ks': 'anything',          // value type irrelevant
      'tutorial_01.ks': { meta: 1 },
      'galgame_demo.ks': null,
    })
    expect(out.map((g) => g.group)).toEqual(['demo', 'tutorial', 'other'])
    expect(out[2].options[0].value).toBe('story.ks')
  })

  it('handles array input, sorting within each group', () => {
    const out = buildSceneOptions(['b_demo.ks', 'a_demo.ks', 'tutorial_02.ks', 'tutorial_01.ks', 'z.ks'])
    expect(out.map((g) => g.group)).toEqual(['demo', 'tutorial', 'other'])
    expect(out[0].options.map((o) => o.value)).toEqual(['a_demo.ks', 'b_demo.ks'])
    expect(out[1].options.map((o) => o.value)).toEqual(['tutorial_01.ks', 'tutorial_02.ks'])
    expect(out[2].options.map((o) => o.value)).toEqual(['z.ks'])
  })

  it('returns [] for empty and missing inputs (defaults to no options)', () => {
    expect(buildSceneOptions(null)).toEqual([])
    expect(buildSceneOptions(undefined)).toEqual([])
    expect(buildSceneOptions({})).toEqual([])
    expect(buildSceneOptions([])).toEqual([])
  })

  it('tolerates non-object, non-array inputs instead of throwing', () => {
    expect(buildSceneOptions('galgame_demo.ks')).toEqual([])   // a bare string is not a map/array
    expect(buildSceneOptions(42)).toEqual([])
    expect(buildSceneOptions(true)).toEqual([])
    expect(() => buildSceneOptions('bytes')).not.toThrow()
  })

  it('omits empty groups entirely (default-empty group semantics)', () => {
    expect(buildSceneOptions({ 'story.ks': {} }).map((g) => g.group)).toEqual(['other'])
    expect(buildSceneOptions({ 'a_demo.ks': {} }).map((g) => g.group)).toEqual(['demo'])
    expect(buildSceneOptions({ 'tutorial_a.ks': {} }).map((g) => g.group)).toEqual(['tutorial'])
  })

  it('supports null-prototype (Object.create(null)) scene maps', () => {
    const map = Object.create(null)
    map['a_demo.ks'] = {}
    map['story.ks'] = {}
    const out = buildSceneOptions(map)
    expect(out.map((g) => g.group)).toEqual(['demo', 'other'])
    expect(out[0].options[0].value).toBe('a_demo.ks')
  })

  it('does not mutate the caller-provided scene array', () => {
    const inputs = ['z_demo.ks', 'a_demo.ks', 'story.ks']
    const snapshot = inputs.slice()
    buildSceneOptions(inputs)
    expect(inputs).toEqual(snapshot) // original order & identity preserved
  })

  it('does not mutate the caller-provided scene object', () => {
    const scenes = { 'galgame_demo.ks': {}, 'story.ks': {} }
    buildSceneOptions(scenes)
    expect(Object.keys(scenes)).toEqual(['galgame_demo.ks', 'story.ks'])
  })

  it('emits fresh option objects (no aliasing across calls)', () => {
    const first = buildSceneOptions({ 'a_demo.ks': {} })
    const second = buildSceneOptions({ 'a_demo.ks': {} })
    expect(first[0].options[0]).not.toBe(second[0].options[0])
    expect(first[0]).not.toBe(second[0])
  })

  it('preserves duplicate keys when the source is an array', () => {
    const out = buildSceneOptions(['galgame_demo.ks', 'galgame_demo.ks'])
    expect(out[0].options.map((o) => o.value)).toEqual(['galgame_demo.ks', 'galgame_demo.ks'])
  })

  it('orders keys case-insensitively via plain sort (uppercase first is acceptable)', () => {
    // Documented behavior: sort() is default lexical (uppercase before lowercase).
    const out = buildSceneOptions(['b_demo.ks', 'B_demo.ks'])
    expect(out[0].options.map((o) => o.value)).toEqual(['B_demo.ks', 'b_demo.ks'])
  })

  it('produces a stable full result set for a mixed bundle', () => {
    const out = buildSceneOptions({
      'story.ks': {}, 'extra/tutorial_mid.ks': {},
      'demo/road_demo.ks': {}, 'alpha_demo.ks': {}, 'tutorial_zz.ks': {},
    })
    expect(out).toEqual([
      { group: 'demo', label: GROUP_LABELS.demo, options: [
        { value: 'alpha_demo.ks', label: 'alpha_demo.ks' },
        { value: 'demo/road_demo.ks', label: 'demo/road_demo.ks' },
      ] },
      { group: 'tutorial', label: GROUP_LABELS.tutorial, options: [
        { value: 'extra/tutorial_mid.ks', label: 'extra/tutorial_mid.ks' },
        { value: 'tutorial_zz.ks', label: 'tutorial_zz.ks' },
      ] },
      { group: 'other', label: GROUP_LABELS.other, options: [
        { value: 'story.ks', label: 'story.ks' },
      ] },
    ])
  })
})
