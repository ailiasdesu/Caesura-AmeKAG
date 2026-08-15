// scene-options.test.js — scene picker grouping logic (round 44).
import { describe, it, expect } from 'vitest'
import { classifyScene, buildSceneOptions, GROUP_LABELS } from './scene-options.js'

describe('classifyScene', () => {
  it('groups tutorial_* scenes into the tutorial group', () => {
    expect(classifyScene('tutorial_01_hello.ks')).toBe('tutorial')
    expect(classifyScene('demo/tutorial/tutorial_05_branching.ks')).toBe('tutorial')
  })

  it('groups *_demo.ks and showcase.ks into the demo group', () => {
    expect(classifyScene('galgame_demo.ks')).toBe('demo')
    expect(classifyScene('full_pipeline_demo.ks')).toBe('demo')
    expect(classifyScene('sma_demo.ks')).toBe('demo')
    expect(classifyScene('showcase.ks')).toBe('demo')
  })

  it('falls back to other for unclassified keys', () => {
    expect(classifyScene('story.ks')).toBe('other')
    expect(classifyScene('intro')).toBe('other')
    expect(classifyScene(null)).toBe('other')
    expect(classifyScene('')).toBe('other')
  })
})

describe('buildSceneOptions', () => {
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

  it('exposes human-readable group labels', () => {
    const out = buildSceneOptions(bundleScenes)
    expect(out[0].label).toBe(GROUP_LABELS.demo)
    expect(out[0].label).toContain('演示')
    expect(out[1].label).toContain('教程')
  })

  it('handles array input and empty/missing input', () => {
    expect(buildSceneOptions(['a_demo.ks'])[0].options[0].value).toBe('a_demo.ks')
    expect(buildSceneOptions([])).toEqual([])
    expect(buildSceneOptions(null)).toEqual([])
    expect(buildSceneOptions(undefined)).toEqual([])
    expect(buildSceneOptions({})).toEqual([])
  })

  it('omits empty groups entirely', () => {
    const out = buildSceneOptions({ 'story.ks': {} })
    expect(out.map((g) => g.group)).toEqual(['other'])
  })
})
