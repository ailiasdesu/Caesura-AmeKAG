// @vitest-environment node
// Web player resource contract: the repo-root static layout the vite
// config (publicDir: '..') serves - /scripts/, /demo/, /assets/, /cache/.
import { describe, it, expect } from 'vitest'
import { readFileSync, existsSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')

describe('web player resource layout', () => {
  it('serves scripts index, story bundle and demo scenes', () => {
    expect(existsSync(join(here, 'scripts-index.json'))).toBe(true)
    expect(existsSync(join(root, 'cache', 'story', 'story.lua'))).toBe(true)
    expect(existsSync(join(root, 'demo', 'galgame_demo.ks'))).toBe(true)
    expect(existsSync(join(root, 'assets', 'bg', 'classroom.png'))).toBe(true)
  })

  it('story bundle lists every demo scene and its assets', () => {
    const story = readFileSync(join(root, 'cache', 'story', 'story.lua'), 'utf8')
    expect(story.includes('galgame_demo.ks')).toBe(true)
    expect(story.includes('full_pipeline_demo.ks')).toBe(true)
    expect(story.includes('sma_demo.ks')).toBe(true)
    expect(story.includes('classroom.png')).toBe(true)
    expect(story.includes('daily.wav')).toBe(true)
  })

  it('scripts-index.json covers the pure-Lua modules', () => {
    const idx = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
    expect(idx['tokenizer']).toBe(true)
    expect(idx['scheduler']).toBe(true)
    expect(idx['kag.schema']).toBe(true)
    expect(idx['layers']).toBe(true)
  })
})
