import {describe,it,expect} from 'vitest'
import {validateStoryBrowserReport,requiredSources,allowFreshViteDependency} from './test-support/run-story-browser-benchmark.mjs'
import {mkdtempSync,mkdirSync,writeFileSync,symlinkSync,rmSync} from 'node:fs'
import {tmpdir} from 'node:os'
import {join} from 'node:path'

function report() {
  const sample={out:'DONE:339:193',wallMs:3000,frames:4000,framesPerMs:4000/3000,
    tokens:339,tokensPerMs:339/3000,memGrowthKB:20,renderedFrames:150,errors:[],audioBefore:'closed',audioAfter:'closed',
    audioContextRetained:true,audioAvailable:false}
  const sources=Object.fromEntries(requiredSources.map(path=>[path,'a'.repeat(64)]))
  return {passed:true,sourceStable:true,browserExited:true,endpointClosed:true,launcherExit:0,
    browser:process.execPath,browserVersion:'Chrome/154.0.0.0',browserSha256:'b'.repeat(64),browserPid:123,node:process.version,
    sourceBefore:sources,sourceAfter:{...sources},
    owner:{actualExit:0,cleanupComplete:true,timedOut:false,python:process.execPath},
    result:{visibility:'visible',disposed:true,audioAvailable:false,audioProfile:'unavailable-real-closed-context',
      warmup:{...sample},samples:Array.from({length:3},()=>({...sample}))}}
}
describe('required real-browser story report',()=>{
  it('retains all three measured samples',()=>{
    const value=report();expect(validateStoryBrowserReport(value)).toBe(value.result.samples)
  })
  it.each([
    ['missing browser',value=>{value.passed=false}],
    ['changed source',value=>{value.sourceStable=false}],
    ['live browser',value=>{value.browserExited=false}],
    ['open endpoint',value=>{value.endpointClosed=false}],
    ['nonzero launcher',value=>{value.launcherExit=1}],
    ['forced cleanup',value=>{value.cleanupFallback=true}],
    ['missing browser path',value=>{delete value.browser}],
    ['missing browser version',value=>{delete value.browserVersion}],
    ['missing browser digest',value=>{delete value.browserSha256}],
    ['missing browser pid',value=>{delete value.browserPid}],
    ['missing node identity',value=>{delete value.node}],
    ['missing source manifest',value=>{delete value.sourceBefore}],
    ['empty source manifest',value=>{value.sourceBefore={};value.sourceAfter={}}],
    ['changed raw source',value=>{value.sourceAfter['web/bridge.js']='c'.repeat(64)}],
    ['missing VM JavaScript',value=>{delete value.sourceBefore['web/node_modules/wasmoon/dist/index.js'];delete value.sourceAfter['web/node_modules/wasmoon/dist/index.js']}],
    ['malformed source digest',value=>{value.sourceBefore['web/bridge.js']='bad';value.sourceAfter['web/bridge.js']='bad'}],
    ['missing tree owner',value=>{delete value.owner}],
    ['failed tree owner',value=>{value.owner.actualExit=1}],
    ['incomplete tree cleanup',value=>{value.owner.cleanupComplete=false}],
    ['owner timeout',value=>{value.owner.timedOut=true}],
    ['hidden page',value=>{value.result.visibility='hidden'}],
    ['missing teardown',value=>{value.result.disposed=false}],
    ['changed audio capability',value=>{value.result.audioAvailable=true}],
    ['missing audio profile',value=>{delete value.result.audioProfile}],
    ['running audio before',value=>{value.result.samples[0].audioBefore='running'}],
    ['resumed audio afterward',value=>{value.result.samples[0].audioAfter='running'}],
    ['replaced audio context',value=>{value.result.samples[0].audioContextRetained=false}],
    ['enabled audio sample',value=>{value.result.samples[0].audioAvailable=true}],
    ['missing sample',value=>{value.result.samples.pop()}],
    ['invalid duration',value=>{value.result.samples[0].wallMs=NaN}],
    ['zero duration',value=>{value.result.samples[0].wallMs=0}],
    ['unfinished story',value=>{value.result.samples[0].out='WAIT:42'}],
    ['token mismatch',value=>{value.result.samples[0].tokens=1}],
    ['no scheduler progress',value=>{value.result.samples[0].frames=0}],
    ['no real render progress',value=>{value.result.samples[0].renderedFrames=0}],
    ['command failure',value=>{value.result.samples[0].errors=[{kind:'error'}]}],
    ['invented throughput',value=>{value.result.samples[0].tokensPerMs=1}],
  ])('rejects %s',(_name,mutate)=>{
    const value=report();mutate(value);expect(()=>validateStoryBrowserReport(value)).toThrow()
  })
})

describe('owned Vite dependency publication boundary',()=>{
  it('permits existing and pending dependencies only inside the owned cache',()=>{
    const owned=mkdtempSync(join(tmpdir(),'caesura-vite-path-'))
    const cache=join(owned,'cache'),outside=join(owned,'outside')
    mkdirSync(cache);mkdirSync(outside)
    const url=file=>'/@fs/'+file.replaceAll('\\','/')
    try {
      expect(allowFreshViteDependency(url(join(cache,'deps','future.js')),cache)).toBe(true)
      mkdirSync(join(cache,'deps'));writeFileSync(join(cache,'deps','ready.js'),'export {}')
      expect(allowFreshViteDependency(url(join(cache,'deps','ready.js')),cache)).toBe(true)
      expect(allowFreshViteDependency(url(join(cache,'deps','future.js.map')),cache)).toBe(true)
      expect(allowFreshViteDependency(url(join(outside,'escape.js')),cache)).toBe(false)
      expect(allowFreshViteDependency(url(join(cache,'..','outside','escape.js')),cache)).toBe(false)
      expect(allowFreshViteDependency(url(join(cache,'deps','secret.json')),cache)).toBe(false)
      symlinkSync(outside,join(cache,'linked'),process.platform==='win32'?'junction':'dir')
      expect(allowFreshViteDependency(url(join(cache,'linked','future.js')),cache)).toBe(false)
      expect(allowFreshViteDependency('/bridge.js',cache)).toBe(false)
    } finally {rmSync(owned,{recursive:true,force:true})}
  })
})
