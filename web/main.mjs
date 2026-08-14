// Caesura Web Player entry — loads scripts via fetch, runs scenes in
// wasmoon. Prefers the pre-baked story bundle (ks_bake --web) for
// zero-parse scene starts; falls back to raw .ks sources.
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'

const stage = document.getElementById('stage')
const logEl = document.getElementById('log')
const statusEl = document.getElementById('status')
const log = (s) => { logEl.textContent += s + '\n'; logEl.scrollTop = logEl.scrollHeight }

const SCRIPTS_BASE = '/scripts/'
const DEMO_BASE = '/demo/'
const STORY_BASE = '/cache/story/story.lua'
const ASSET_BASE = '/assets/'

const player = await createPlayer({ scriptsBase: SCRIPTS_BASE })
log('engine loaded; kag table ready')

const renderer = new DomRenderer(player.core, stage)
let storyBundle = null

const syncTextures = () => {
  for (const [id, t] of player.core.textures) {
    renderer.setTextureUrl(id, ASSET_BASE + t.path)
  }
}

async function loadStoryBundle() {
  try {
    const src = await (await fetch(STORY_BASE)).text()
    player.lua.global.set('STORY_SRC', src)
    const SQ = String.fromCharCode(39)
    const code = '  local chunk = assert(load(STORY_SRC, ' + SQ + '@story.lua' + SQ + ', ' + SQ + 't' + SQ + ', _ENV))' + String.fromCharCode(10) + '  return chunk()'
    const bundle = await player.lua.doString(code)
    storyBundle = bundle
    log('story bundle: ' + Object.keys(bundle.scenes).length + ' scenes, ' + bundle.assets.length + ' assets')
  } catch (e) {
    log('story bundle unavailable: ' + String(e).slice(0, 80))
  }
}

async function runScene(name) {
  log('running ' + name)
  statusEl.textContent = 'running…'
  let out
  if (storyBundle && storyBundle.scenes[name]) {
    out = await player.runFromBundle(storyBundle, name, { autoClick: false })
  } else {
    const ks = await (await fetch(DEMO_BASE + name)).text()
    out = await player.runScene(ks, name, { autoClick: false })
  }
  syncTextures()
  await renderer.render()
  statusEl.textContent = 'parked: ' + out
  log('result: ' + out)
}

async function advance() {
  const sel = document.getElementById('scene').value
  log('click')
  await player.click()
  let out
  if (storyBundle && storyBundle.scenes[sel]) {
    out = await player.runFromBundle(storyBundle, sel, { autoClick: false, maxFrames: 5000 })
  } else {
    const ks = await (await fetch(DEMO_BASE + sel)).text()
    out = await player.runScene(ks, sel, { autoClick: false, maxFrames: 5000 })
  }
  syncTextures()
  await renderer.render()
  statusEl.textContent = 'advance: ' + out
  log('advance: ' + out)
}

await loadStoryBundle()

document.getElementById('run').addEventListener('click', () => {
  void runScene(document.getElementById('scene').value)
})
document.getElementById('advance').addEventListener('click', () => {
  void advance()
})

// render loop: sync core state to DOM every frame (CSS transitions
// interpolate sprite moves/fades between renders).
const frame = () => {
  renderer.render()
  requestAnimationFrame(frame)
}
requestAnimationFrame(frame)

void runScene('galgame_demo.ks')
