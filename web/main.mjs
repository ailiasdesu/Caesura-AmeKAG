// Caesura Web Player entry — loads scripts via fetch, runs scenes in
// wasmoon. Prefers the pre-baked story bundle (ks_bake --web) for
// zero-parse scene starts; falls back to raw .ks sources.
import { createPlayer } from './bridge.js'
import { DomRenderer } from './dom-renderer.js'
import { buildSceneOptions } from './scene-options.js'

const stage = document.getElementById('stage')
const logEl = document.getElementById('log')
const statusEl = document.getElementById('status')
const endingsEl = document.getElementById('endings')
const endingsCount = document.getElementById('endings-count')
const audioStatusEl = document.getElementById('audio-status')
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
    populateScenePicker(bundle.scenes)
    populateFallbackScenes()
  } catch (e) {
    log('story bundle unavailable: ' + String(e).slice(0, 80))
  }
}


function populateScenePicker(scenes) {
  const sel = document.getElementById('scene')
  sel.textContent = ''
  for (const group of buildSceneOptions(scenes)) {
    const og = document.createElement('optgroup')
    og.label = group.label
    for (const opt of group.options) {
      const o = document.createElement('option')
      o.value = opt.value
      o.textContent = opt.label
      og.appendChild(o)
    }
    sel.appendChild(og)
  }
}

// When the bundle is unavailable, offer raw .ks files from /demo (flat list).
const FALLBACK_SCENES = [
  'galgame_demo.ks', 'full_pipeline_demo.ks', 'sma_demo.ks', 'showcase.ks',
  'tutorial/tutorial_01_hello.ks', 'tutorial/tutorial_02_text.ks',
  'tutorial/tutorial_03_layers.ks', 'tutorial/tutorial_04_audio.ks',
  'tutorial/tutorial_05_branching.ks', 'tutorial/tutorial_06_effects.ks',
]

function populateFallbackScenes() {
  if (document.getElementById('scene').options.length > 0) return
  const sel = document.getElementById('scene')
  sel.textContent = ''
  for (const name of FALLBACK_SCENES) {
    const o = document.createElement('option')
    o.value = name
    o.textContent = name
    sel.appendChild(o)
  }
}

let lastEndingsLen = -1
const syncEndings = () => {
  const list = player.core.endings || []
  if (list.length === lastEndingsLen) return
  lastEndingsLen = list.length
  endingsCount.textContent = String(list.length)
  endingsEl.textContent = ''
  for (const e of list) {
    const row = document.createElement('div')
    row.className = 'ending-entry'
    row.textContent = (e.id || '?') + ' — ' + (e.name || '')
    endingsEl.appendChild(row)
  }
}

const syncAudioStatus = () => {
  const bus = player.core.audioBus
  const parts = []
  if (bus.bgm && bus.bgm.playing) parts.push('BGM: ' + bus.bgm.path.split('/').pop())
  const sePlaying = (bus.se || []).filter((x) => x && x.playing)
  if (sePlaying.length > 0) parts.push('SE: ' + sePlaying.length)
  if (bus.voice && bus.voice.playing) parts.push('VOICE: ' + bus.voice.path.split('/').pop())
  const txt = parts.length > 0 ? parts.join(' · ') : '—'
  if (audioStatusEl.textContent !== txt) audioStatusEl.textContent = txt
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
  // VN semantics: clicking commits the visible page to history
  if (player.core.draws.length > 0) {
    const text = player.core.draws.map((d) => d.t).join('')
    player.core.setDraws(player.core.draws.map((d) => ({ ...d })))
  }
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
document.getElementById('auto').addEventListener('click', () => {
  autoMode = !autoMode
  document.getElementById('auto').textContent = autoMode ? '⏸ Auto' : '⏩ Auto'
  if (autoMode) scheduleAuto()
  else if (autoTimer) clearTimeout(autoTimer)
})

// --- backlog (VN history) ---
const backlogEl = document.getElementById('backlog')
const backlogCount = document.getElementById('backlog-count')
let lastBacklogLen = -1
const syncBacklog = () => {
  const bl = player.core.backlog
  if (bl.length === lastBacklogLen) return
  lastBacklogLen = bl.length
  backlogCount.textContent = String(bl.length)
  backlogEl.textContent = ''
  bl.forEach((entry, i) => {
    const row = document.createElement('div')
    row.className = 'backlog-entry'
    row.innerHTML = '<span class="bl-line">' + (i + 1) + '</span>' + entry.text.replace(/</g, '&lt;')
    row.title = 'backlog ' + (i + 1)
    backlogEl.appendChild(row)
  })
  backlogEl.scrollTop = backlogEl.scrollHeight
}

// --- auto-advance ---
let autoMode = false
let autoTimer = null
const scheduleAuto = () => {
  if (!autoMode) return
  autoTimer = setTimeout(() => { void advance(); scheduleAuto() }, 1200)
}

// render loop: sync core state to DOM every frame (CSS transitions
// interpolate sprite moves/fades between renders).
const frame = () => {
  renderer.render()
  syncBacklog()
  syncEndings()
  syncAudioStatus()
  requestAnimationFrame(frame)
}
requestAnimationFrame(frame)

void runScene('galgame_demo.ks')
