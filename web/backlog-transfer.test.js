// @vitest-environment jsdom
import {beforeAll,afterEach,describe,it,expect,vi} from 'vitest'
import {Lua} from 'wasmoon'
import {readFileSync} from 'node:fs'
import {dirname,join} from 'node:path'
import {fileURLToPath} from 'node:url'
import {createBacklogReader} from './backlog-transfer.js'
import {createPlayer} from './bridge.js'
import {installCanvasHost} from './test-support/canvas-host.js'
import {createRepositoryFetch,repositoryAssetUrl} from './test-support/repository-fetch.js'
const here=dirname(fileURLToPath(import.meta.url)),root=join(here,'..')
let factory,lua,player,restoreCanvas
beforeAll(async()=>{factory=await Lua.load({wasmFile:join(here,'node_modules/wasmoon/dist/glue.wasm')})})
afterEach(async()=>{
 vi.restoreAllMocks()
 try{await player?.dispose()}finally{player=null;lua?.global.close();lua=null;restoreCanvas?.();restoreCanvas=null}
})
async function fixture(source){lua=factory.createState();await lua.doString(source);return createBacklogReader(lua)}
const jsonCopy=value=>JSON.parse(JSON.stringify(value))

describe('bounded ASCII backlog transfer with real Wasmoon',()=>{
 it('preserves controls, numeric values and empty-table projection exactly as the old consumer',async()=>{
  const read=await fixture(`
   local controls={};for byte=1,31 do controls[#controls+1]=string.char(byte)end
   __SCENE_BACKLOG={{},{
    {t='quote" slash'..string.char(92)..' literal'..string.char(92)..'u0041'..table.concat(controls)..string.char(127),x=1.125,y=-0.0},
    {t='next',x=-123.456,y=math.pi}
   },{}}
  `)
  const old=jsonCopy(lua.global.get('__SCENE_BACKLOG'))
  const get=vi.spyOn(lua.global,'get')
  const result=read()
  expect(result.encoded).toBe(true);expect(result.pages).toEqual(old)
  expect(Object.is(result.pages[1][0].y,-0)).toBe(false)
  expect(get).not.toHaveBeenCalled()
  await lua.doString('__SCENE_BACKLOG={}')
  expect(read()).toEqual({pages:{},encoded:true})
 })
 it('returns settled detached pages and observes subsequent live edits at each read',async()=>{
  const read=await fixture("__SCENE_BACKLOG={{{t='OLD',x=1,y=2}}}")
  const first=read(),second=read()
  expect(first.pages).toEqual(second.pages);expect(first.pages).not.toBe(second.pages)
  expect(first.pages[0][0]).not.toBe(second.pages[0][0])
  await lua.doString("__SCENE_BACKLOG[1][1].t='NEW';__SCENE_BACKLOG[1][1].x=3")
  expect(first.pages[0][0]).toEqual({t:'OLD',x:1,y:2})
  first.pages[0][0].t='CLIENT'
  expect(read().pages[0][0]).toEqual({t:'NEW',x:3,y:2})
 })
 it.each([
  ['absent',"__SCENE_BACKLOG=nil"],
  ['hole',"__SCENE_BACKLOG={[2]={}}"],
  ['outer metatable',"__SCENE_BACKLOG=setmetatable({},{})"],
  ['page metatable',"__SCENE_BACKLOG={setmetatable({},{})}"],
  ['row metatable',"__SCENE_BACKLOG={{setmetatable({t='x',x=1,y=2},{})}}"],
  ['extra field',"__SCENE_BACKLOG={{{t='x',x=1,y=2,extra=3}}}"],
  ['missing field',"__SCENE_BACKLOG={{{t='x',x=1}}}"],
  ['NaN',"__SCENE_BACKLOG={{{t='x',x=0/0,y=2}}}"],
  ['Infinity',"__SCENE_BACKLOG={{{t='x',x=math.huge,y=2}}}"],
  ['unsafe coordinate',"__SCENE_BACKLOG={{{t='x',x=9007199254740992,y=2}}}"],
  ['minimum Lua integer',"__SCENE_BACKLOG={{{t='x',x=math.mininteger,y=2}}}"],
  ['NUL',"__SCENE_BACKLOG={{{t='x'..string.char(0)..'tail',x=1,y=2}}}"],
  ['invalid UTF8',"__SCENE_BACKLOG={{{t=string.char(255),x=1,y=2}}}"],
  ['leading BOM',"__SCENE_BACKLOG={{{t=string.char(239,187,191)..'BOM',x=1,y=2}}}"],
  ['Unicode',"__SCENE_BACKLOG={{{t='中文😀',x=1,y=2}}}"],
  ['function',"__SCENE_BACKLOG={{{t=function()return 17 end,x=1,y=2}}}"],
  ['thread',"__SCENE_BACKLOG={{{t=coroutine.create(function()end),x=1,y=2}}}"],
  ['cycle',"__SCENE_BACKLOG={};__SCENE_BACKLOG[1]=__SCENE_BACKLOG"],
  ['string limit',"__SCENE_BACKLOG={{{t=string.rep('x',262145),x=1,y=2}}}"],
  ['node limit',"local rows={};for i=1,16400 do rows[i]={t='x',x=1,y=2}end;__SCENE_BACKLOG={rows}"],
  ['byte limit',"local rows={};for i=1,5 do rows[i]={t=string.rep('x',262144),x=1,y=2}end;__SCENE_BACKLOG={rows}"],
 ])('retains the exact legacy reader for %s',async(_name,source)=>{
  const read=await fixture(source)
  const oldGet=lua.global.get.bind(lua.global)
  let observed
  const get=vi.spyOn(lua.global,'get').mockImplementation(name=>{observed=oldGet(name);return observed})
  const result=read()
  expect(result.encoded).toBe(false);expect(result.pages).toBe(observed)
  expect(get).toHaveBeenCalledOnce();expect(get).toHaveBeenCalledWith('__SCENE_BACKLOG')
 })
 it('accepts the original single-string boundary and safe finite coordinate boundaries',async()=>{
  const read=await fixture("__SCENE_BACKLOG={{{t=string.rep('x',262144),x=9007199254740991,y=-9007199254740991}}}")
  expect(read()).toEqual({encoded:true,pages:jsonCopy(lua.global.get('__SCENE_BACKLOG'))})
 })
 it('retains native codec builtins after a game changes global libraries',async()=>{
  const read=await fixture("__SCENE_BACKLOG={{{t='x',x=1,y=2}}}")
  await lua.doString("string.format=function()error('game replacement')end;string.gsub=string.format;string.find=string.format;table.concat=string.format;math.abs=string.format;rawget=string.format;next=string.format;type=string.format;getmetatable=string.format;_G={}")
  expect(read()).toEqual({encoded:true,pages:[[{t:'x',x:1,y:2}]]})
 })
 it('propagates an unexpected expired Lua state error',async()=>{
  const read=await fixture("__SCENE_BACKLOG={{{t='x',x=1,y=2}}}")
  lua.global.close()
  expect(()=>read()).toThrow()
 })
 it('propagates an unexpected parse failure',async()=>{
  const read=await fixture("__SCENE_BACKLOG={{{t='x',x=1,y=2}}}")
  vi.spyOn(JSON,'parse').mockImplementation(()=>{throw new Error('injected parse failure')})
  expect(()=>read()).toThrow(/injected parse failure/)
 })
})

async function actualPlayer(){
 restoreCanvas=installCanvasHost()
 const index=JSON.parse(readFileSync(join(here,'scripts-index.json'),'utf8'))
 const store=new Map()
 player=await createPlayer({scriptsBase:'http://local/scripts/',fetchImpl:createRepositoryFetch(root,index),assetUrl:repositoryAssetUrl,audioAssetUrl:repositoryAssetUrl,langBase:'http://local/assets/lang/',wasmFile:join(here,'node_modules/wasmoon/dist/glue.wasm'),storageBackend:{get:key=>store.get(key)??null,set:(key,value)=>{store.set(key,value);return true},del:key=>store.delete(key)}})
 return player
}
describe('backlog transfer at the actual bridge publication boundary',()=>{
 it('keeps page order, duplicate suppression, Unicode fallback and the full settled context',async()=>{
  await actualPlayer()
  const source='[set f.marker=7]\n[ch text="FIRST"]\n[p]\n[ch text="FIRST"]\n[p]\n[ch text="SECOND"]\n[p]\n[end]'
  const get=vi.spyOn(player.lua.global,'get')
  expect(await player.runScene(source,'backlog-transfer.ks',{autoClick:true})).toMatch(/^DONE:/)
  expect(player.core.backlog.map(page=>page.text)).toEqual(['FIRST','SECOND'])
  expect(get.mock.calls.filter(([name])=>name==='__SCENE_BACKLOG')).toHaveLength(0)
  const retained=structuredClone(player.core.backlog)
  expect(player._ctx.f.marker).toBe(7)
  expect(player._ctx._undoStack.length).toBeGreaterThan(0)
  await player.lua.doString("__CTXREF.f.marker=99;__CTXREF.text_state.draws={}")
  expect(player._ctx.f.marker).toBe(7);expect(player.core.backlog).toEqual(retained)
  await player.runScene('[ch text="中文"]\n[p]\n[end]','unicode-backlog.ks',{autoClick:true})
  expect(get.mock.calls.filter(([name])=>name==='__SCENE_BACKLOG')).toHaveLength(1)
  expect(player.core.backlog.map(page=>page.text)).toEqual(['FIRST','SECOND','中文'])
 })
 it('publishes a restored history through the existing transaction and resumes its page',async()=>{
  await actualPlayer()
  const source='[ch text="ONE"]\n[p]\n[ch text="TWO"]\n[p]\n[ch text="THREE"]\n[p]\n[end]'
  expect(await player.runScene(source,'backlog-restore.ks',{autoClick:true})).toMatch(/^DONE:/)
  const savedHistory=structuredClone(player.core.backlog)
  expect(await player.saveCurrent(91)).toBe(true)
  await player.runScene('[ch text="OTHER"]\n[p]\n[end]','other.ks',{autoClick:true})
  expect(await player.loadSlot(91,{sceneSources:{'backlog-restore.ks':source},autoClick:false})).toMatch(/^(WAIT|DONE):/)
  expect(player.core.backlog.map(page=>page.text)).toEqual(savedHistory.map(page=>page.text))
  expect(player._ctx.current_scene).toBe('backlog-restore.ks')
 })
})
