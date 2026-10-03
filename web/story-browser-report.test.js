import {describe,it,expect} from 'vitest'
import {validateStoryBrowserReport,requiredSources,allowFreshViteDependency,viteFsPath,browserFailureSummary} from './test-support/run-story-browser-benchmark.mjs'
import {mkdtempSync,mkdirSync,writeFileSync,symlinkSync,rmSync} from 'node:fs'
import {tmpdir} from 'node:os'
import {join,posix} from 'node:path'
import {createHash} from 'node:crypto'

function report() {
  const sample={out:'DONE:339:193',wallMs:3000,frames:4000,framesPerMs:4000/3000,
    tokens:339,tokensPerMs:339/3000,memGrowthKB:20,renderedFrames:150,errors:[],audioBefore:'closed',audioAfter:'closed',
    audioContextRetained:true,audioAvailable:false}
  const sources=Object.fromEntries(requiredSources.map(path=>[path,'a'.repeat(64)]))
  return {passed:true,sourceStable:true,browserExited:true,endpointClosed:true,launcherExit:0,
    browser:process.execPath,browserVersion:'Chrome/154.0.0.0',browserSha256:'b'.repeat(64),browserPid:123,node:process.version,
    sourceBefore:sources,sourceAfter:{...sources},sourceManifestSha256:createHash('sha256').update(JSON.stringify(sources)).digest('hex'),
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
    ['wrong source digest',value=>{value.sourceManifestSha256='d'.repeat(64)}],
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
  it.each([
    ['/tmp/owned/cache/deps/wasmoon.js','/tmp/owned/cache/deps/wasmoon.js'],
    ['E:/owned/cache/deps/wasmoon.js','E:/owned/cache/deps/wasmoon.js'],
    ['/tmp/owned/cache/deps/../wasmoon.js','/tmp/owned/cache/wasmoon.js'],
  ])('decodes Vite generated URL for %s independently of the host OS',(file,expected)=>{
    expect(viteFsPath(posix.join('/@fs/',file))).toBe(expected)
  })
  it('rejects non-Vite and drive-relative paths',()=>{
    expect(viteFsPath('/tmp/file.js')).toBeNull()
    expect(viteFsPath('/@fs/E:relative.js')).toBeNull()
    expect(viteFsPath('/@fs/E:\\escape.js')).toBeNull()
  })
  it('permits existing and pending dependencies only inside the owned cache',()=>{
    const owned=mkdtempSync(join(tmpdir(),'caesura-vite-path-'))
    const cache=join(owned,'cache'),outside=join(owned,'outside')
    mkdirSync(cache);mkdirSync(outside)
    const url=file=>posix.join('/@fs/',file.replaceAll('\\','/'))
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

describe('bounded browser failure diagnostics',()=>{
  it('retains original error, request failure and cleanup fields',()=>{
    const value={passed:false,error:'Script HTTP 404: /@fs/tmp/owned/wasmoon.js',
      diagnostics:[{kind:'http-denied',detail:'/@fs/tmp/owned/wasmoon.js'}],
      sourceStable:true,endpointClosed:true,browserExited:true,launcherExit:0,cleanupFallback:false}
    const summary=browserFailureSummary(value)
    expect(summary.error).toBe(value.error)
    expect(summary.diagnostics).toEqual(value.diagnostics)
    expect(summary.endpointClosed).toBe(true)
    expect(summary.browserExited).toBe(true)
    expect(summary.launcherExit).toBe(0)
  })
  it('bounds diagnostics while reporting omitted entries',()=>{
    const summary=browserFailureSummary({error:'e'.repeat(10000),
      diagnostics:Array.from({length:64},()=>({kind:'network',detail:'d'.repeat(5000)}))})
    expect(summary.error).toHaveLength(4096)
    expect(summary.diagnostics).toHaveLength(16)
    expect(summary.diagnosticCount).toBe(64)
    expect(JSON.stringify(summary).length).toBeLessThan(23000)
  })
})
