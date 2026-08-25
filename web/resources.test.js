// @vitest-environment node
// Web player resource contract: the repo-root static layout the vite
// config (publicDir: '..') serves - /scripts/, /demo/, /assets/, /cache/.
//
// Sprint 1 / t3 — honesty of the guards:
//   Three cases used to bail out with a bare `if (!existsSync(x)) return`,
//   which vitest reports as PASSED. A missing story bundle or a missing
//   web/dist (= the packaging / build chain never ran, or ran and failed)
//   therefore produced a green run that asserted nothing. Those guards are
//   now vitest.skipIf with the exact regeneration command in the case name,
//   so a skipped case is visible in the report and tells you how to make it
//   run. Every artifact-dependent assertion inside a case is a REAL
//   assertion again (no nested existsSync escape hatches), and the two
//   cases that need no build artifact assert unconditionally.
import { describe, it, expect } from 'vitest'
import { readFileSync, existsSync, readdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = join(here, '..')
const readdirSafe = (d) => { try { return readdirSync(d) } catch { return [] } }

// Generated (gitignored) artifacts and the exact commands that produce them.
const ROOT_STORY = join(root, 'cache', 'story', 'story.lua')
const DIST = join(here, 'dist')
const DIST_STORY = join(DIST, 'cache', 'story', 'story.lua')
const BAKE_CMD = 'external/lua/lua.exe scripts/ks_bake.lua --dir demo --web cache/story'
const BUILD_CMD = 'cd web && npm run build'
const hasRootStory = existsSync(ROOT_STORY)
const hasDist = existsSync(DIST)

describe('web player resource layout', () => {
  // Committed / repo-tracked inputs: always asserted, no artifact needed.
  it('serves the scripts index, demo scenes and shared assets', () => {
    expect(existsSync(join(here, 'scripts-index.json'))).toBe(true)
    expect(existsSync(join(root, 'demo', 'galgame_demo.ks'))).toBe(true)
    expect(existsSync(join(root, 'assets', 'bg', 'classroom.png'))).toBe(true)
  })

  it.skipIf(!hasRootStory)(
    'story bundle exists and lists every demo scene and its assets [requires: ' + BAKE_CMD + ']',
    () => {
      expect(existsSync(ROOT_STORY)).toBe(true)
      const story = readFileSync(ROOT_STORY, 'utf8')
      expect(story.includes('galgame_demo.ks')).toBe(true)
      expect(story.includes('full_pipeline_demo.ks')).toBe(true)
      expect(story.includes('sma_demo.ks')).toBe(true)
      expect(story.includes('classroom.png')).toBe(true)
      expect(story.includes('daily.wav')).toBe(true)
    },
  )

  it('scripts-index.json covers the pure-Lua modules', () => {
    const idx = JSON.parse(readFileSync(join(here, 'scripts-index.json'), 'utf8'))
    expect(idx['tokenizer']).toBe(true)
    expect(idx['scheduler']).toBe(true)
    expect(idx['kag.schema']).toBe(true)
    expect(idx['layers']).toBe(true)
  })

  it.skipIf(!hasDist)(
    'dist build layout: runtime dirs copied, bundle under web-assets [requires: ' + BUILD_CMD + ']',
    () => {
      // Round 44: vite build must produce a self-contained static site.
      expect(existsSync(join(DIST, 'index.html'))).toBe(true)
      expect(existsSync(join(DIST, 'scripts', 'scheduler.lua'))).toBe(true)
      expect(existsSync(join(DIST, 'demo', 'tutorial', 'tutorial_01_hello.ks'))).toBe(true)
      // vite.config.js RUNTIME_DIRS copies 'cache/story' unconditionally, so a
      // dist that exists without the story bundle IS a broken build — assert it
      // instead of skipping past it (that was the silent-failure hole).
      expect(existsSync(DIST_STORY)).toBe(true)
      expect(existsSync(join(DIST, 'assets', 'bg', 'classroom.png'))).toBe(true)
      // vite bundle must NOT collide with the game assets/ directory:
      // bundle chunks live under dist/web-assets/, never inside dist/assets/.
      const webAssets = existsSync(join(DIST, 'web-assets'))
        ? readdirSafe(join(DIST, 'web-assets')).filter((f) => f.endsWith('.js'))
        : []
      expect(webAssets.length).toBeGreaterThan(0)
      const inGameAssets = existsSync(join(DIST, 'assets'))
        ? readdirSafe(join(DIST, 'assets')).filter((f) => f.endsWith('.js'))
        : []
      expect(inGameAssets.length).toBe(0)
    },
  )

  it.skipIf(!hasDist)(
    'dist story bundle still carries the tutorial scenes (round 43/44) [requires: ' + BUILD_CMD + ']',
    () => {
      expect(existsSync(DIST_STORY)).toBe(true)
      const story = readFileSync(DIST_STORY, 'utf8')
      expect(story.includes('tutorial_01_hello.ks')).toBe(true)
      expect(story.includes('tutorial_06_effects.ks')).toBe(true)
    },
  )
})

// Make a skip loud in the console as well as in the vitest report, so a run
// with missing artifacts can never be read as "everything was verified".
if (!hasRootStory || !hasDist) {
  const missing = []
  if (!hasRootStory) missing.push('cache/story/story.lua (run: ' + BAKE_CMD + ')')
  if (!hasDist) missing.push('web/dist (run: ' + BUILD_CMD + ')')
  // eslint-disable-next-line no-console
  console.warn('[resources.test] artifact-dependent cases SKIPPED — missing: ' + missing.join('; '))
}
