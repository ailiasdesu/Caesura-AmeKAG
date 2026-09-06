// @vitest-environment jsdom
import { afterAll, beforeAll, expect, it } from 'vitest'
import { existsSync, readFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'

const here = dirname(fileURLToPath(import.meta.url))
let player
beforeAll(async () => {
  player = await createPlayer({
    scriptsBase: 'http://local/scripts/',
    langBase: 'http://local/assets/lang/',
    wasmFile: join(here, 'node_modules/wasmoon/dist/glue.wasm'),
    fetchImpl: async url => {
      const pathname = new URL(url).pathname
      const path = pathname === '/scripts/index.json'
        ? join(here, 'scripts-index.json') : join(here, '..', pathname.slice(1))
      return {
        ok: existsSync(path), status: existsSync(path) ? 200 : 404,
        text: async () => existsSync(path) ? readFileSync(path, 'utf8') : '',
        json: async () => JSON.parse(readFileSync(path, 'utf8')),
      }
    },
  })
})
afterAll(async () => { await player?.dispose() })

async function styledPage() {
  const name = 'language-render.ks'
  const source = [
    '[i18n language="zh"]',
    '[ch text="{s}{settings}{/s}" opacity=128 a=204]',
    '[ruby text="漢字" ruby="かんじ"]',
    '[p]',
  ].join('\n')
  expect(await player.runScene(source, name)).toMatch(/^WAIT:/)
  expect(await player.runScene('', name, { advance: true, advanceScene: name })).toMatch(/^WAIT:/)
  expect(player.core.draws).toEqual(expect.arrayContaining([
    expect.objectContaining({ t: '设置', st: 1, a: 102 }),
    expect.objectContaining({ t: '漢字', ruby: 'かんじ', a: 128 }),
  ]))
}

it('setLanguage preserves rendered opacity and strike while translating a real scene', async () => {
  await styledPage()
  expect(await player.setLanguage('en')).toBe('en')
  expect(player.core.draws).toEqual(expect.arrayContaining([
    expect.objectContaining({ t: 'Settings', st: 1, a: 102 }),
  ]))
  expect(player.core.draws.some(draw => draw.t === '设置')).toBe(false)
})

it('setLanguage preserves ruby and strike in the actual DOM-facing publication', async () => {
  await styledPage()
  expect(await player.setLanguage('en')).toBe('en')
  expect(player.core.draws.find(draw => draw.t === '漢字')).toMatchObject({ ruby: 'かんじ', a: 128 })
  const stage = document.createElement('div')
  new DomRenderer(player.core, stage).render()
  expect(stage.querySelector('ruby rt')?.textContent).toBe('かんじ')
  const translated = [...stage.querySelectorAll('.caesura-message span')].find(span => span.textContent === 'Settings')
  expect(translated?.style.textDecoration).toBe('line-through')
})

it('setLanguage does not publish fully transparent scene text', async () => {
  expect(await player.runScene('[i18n language="zh"]\n[ch text="{settings}" opacity=0]', 'hidden-language.ks')).toMatch(/^WAIT:/)
  expect(player.core.draws).toEqual([])
  expect(await player.setLanguage('en')).toBe('en')
  expect(player.core.draws).toEqual([])
  expect(player.core.textBuffer).toBe('')
})

it('setLanguage respects partial reveal for script-owned text without a translation source', async () => {
  // Script-owned draws need not have page_src. Unlike translated page sources,
  // these draws are not sealed by relocalize_page and still require clipping.
  const source = [
    '[iscript]',
    'kag.text(ctx, {text="VISIBLE-HIDDEN"})',
    'ctx.text_state.page_src = {}',
    'ctx.text_state.reveal_chars = 3',
    'ctx.reveal = nil',
    '[endscript]',
    '[p]',
  ].join('\n')
  expect(await player.runScene(source, 'partial-language.ks')).toMatch(/^WAIT:/)
  expect(player.core.draws.map(draw => draw.t).join('')).toBe('VIS')
  expect(await player.setLanguage('ja')).toBe('ja')
  expect(player.core.draws.map(draw => draw.t).join('')).toBe('VIS')
})
