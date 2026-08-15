// @vitest-environment node
// Web player resource contract: the repo-root static layout the vite
// config (publicDir: '..') serves - /scripts/, /demo/, /assets/, /cache/.
import { describe, it, expect } from 'vitest'
import { readFileSync, existsSync, readdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')
const readdirSafe = (d) => { try { return readdirSync(d) } catch { return [] } }

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

  it('dist build layout: runtime dirs copied, bundle under web-assets', () => {
    // Round 44: vite build must produce a self-contained static site.
    const dist = join(here, 'dist')
    expect(existsSync(join(dist, 'index.html'))).toBe(true)
    expect(existsSync(join(dist, 'scripts', 'scheduler.lua'))).toBe(true)
    expect(existsSync(join(dist, 'demo', 'tutorial', 'tutorial_01_hello.ks'))).toBe(true)
    expect(existsSync(join(dist, 'cache', 'story', 'story.lua'))).toBe(true)
    expect(existsSync(join(dist, 'assets', 'bg', 'classroom.png'))).toBe(true)
    // vite bundle must NOT collide with the game assets/ directory:
    // bundle chunks live under dist/web-assets/, never inside dist/assets/.
    const webAssets = existsSync(join(dist, 'web-assets'))
      ? readdirSafe(join(dist, 'web-assets')).filter((f) => f.endsWith('.js'))
      : []
    expect(webAssets.length).toBeGreaterThan(0)
    const inGameAssets = existsSync(join(dist, 'assets'))
      ? readdirSafe(join(dist, 'assets')).filter((f) => f.endsWith('.js'))
      : []
    expect(inGameAssets.length).toBe(0)
  })

  it('dist story bundle still carries the tutorial scenes (round 43/44)', () => {
    const story = readFileSync(join(here, 'dist', 'cache', 'story', 'story.lua'), 'utf8')
    expect(story.includes('tutorial_01_hello.ks')).toBe(true)
    expect(story.includes('tutorial_06_effects.ks')).toBe(true)
  })

})