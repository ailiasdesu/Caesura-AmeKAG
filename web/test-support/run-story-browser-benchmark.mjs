// Own one real browser and Vite instance; never reuse the user's CDP/profile.
import {spawn, spawnSync} from 'node:child_process'
import {readFileSync, writeFileSync, existsSync, mkdirSync, mkdtempSync, readdirSync, realpathSync, lstatSync} from 'node:fs'
import {dirname, join, resolve, relative, isAbsolute} from 'node:path'
import {fileURLToPath, pathToFileURL} from 'node:url'
import {tmpdir} from 'node:os'
import {createHash} from 'node:crypto'

const here = dirname(fileURLToPath(import.meta.url)), web = resolve(here, '..'), root = resolve(web, '..')
const sleep = ms => new Promise(yes => setTimeout(yes, ms))
const hash = data => createHash('sha256').update(data).digest('hex')
const ensure = (condition, message) => { if (!condition) throw new Error(message) }
const inside = (parent, file) => { const rel = relative(parent, file); return rel !== '' && !rel.startsWith('..') && !isAbsolute(rel) }
export const requiredSources = ['web/bridge.js', 'web/dom-renderer.js', 'web/adapter-core.js', 'web/runner-bridge.js',
  'web/package-lock.json', 'web/scripts-index.json', 'web/perf-baseline.test.js',
  'web/test-support/story-browser-benchmark.js', 'web/test-support/run-story-browser-benchmark.mjs',
  'web/node_modules/wasmoon/dist/index.js', 'web/node_modules/wasmoon/dist/glue.wasm',
  'web/node_modules/wasmoon/package.json', 'demo/example_game/story.ks', 'demo/caesura.project.json',
  'tests/scripts/run_story_browser_benchmark.py', 'scripts/validation_process.py']

export function allowFreshViteDependency(path, cacheRoot) {
  if(!path.startsWith('/@fs/') || !/\.(js|map)$/.test(path))return false
  const file=resolve(path.slice(5)), cache=resolve(cacheRoot)
  if(!inside(cache,file))return false
  try {
    const canonicalCache=realpathSync(cache)
    let ancestor=file
    // Vite publishes the optimized deps after serving the rewritten import.
    // Let Vite await that publication, but only below our existing owned root.
    for(;;){
      try{lstatSync(ancestor);break}
      catch(error){if(error.code!=='ENOENT')return false;ancestor=dirname(ancestor)}
    }
    const canonicalAncestor=realpathSync(ancestor)
    return canonicalAncestor===canonicalCache || inside(canonicalCache,canonicalAncestor)
  }catch{return false}
}

export function validateStoryBrowserReport(report) {
  ensure(report?.passed === true && !report.error, 'Browser benchmark failed')
  ensure(report.sourceStable === true && report.browserExited === true && report.endpointClosed === true,
    'Browser benchmark source or process ownership incomplete')
  ensure(report.launcherExit === 0 && !report.cleanupFallback, 'Browser launcher failed or required forced cleanup')
  ensure(report.owner?.actualExit === 0 && report.owner.cleanupComplete === true
    && report.owner.timedOut === false && typeof report.owner.python === 'string'
    && isAbsolute(report.owner.python), 'Required process-tree owner receipt missing')
  ensure(typeof report.browser === 'string' && isAbsolute(report.browser) && report.browser.trim().length > 0
    && typeof report.browserVersion === 'string' && report.browserVersion.trim().length > 0
    && /^[0-9a-f]{64}$/.test(report.browserSha256 || '')
    && Number.isSafeInteger(report.browserPid) && report.browserPid > 0
    && /^v\d+\./.test(report.node || ''), 'Browser identity is incomplete')
  for(const manifest of [report.sourceBefore, report.sourceAfter]) {
    ensure(manifest && typeof manifest === 'object' && !Array.isArray(manifest)
      && Object.keys(manifest).length >= requiredSources.length, 'Source manifest is empty')
    ensure(requiredSources.every(path => /^[0-9a-f]{64}$/.test(manifest[path] || ''))
      && Object.entries(manifest).every(([path,digest]) => !isAbsolute(path) && !path.split('/').includes('..')
        && !path.includes('\\') && /^[0-9a-f]{64}$/.test(digest)), 'Required source identity missing')
  }
  ensure(JSON.stringify(report.sourceBefore) === JSON.stringify(report.sourceAfter), 'Source manifests changed')
  ensure(report.result?.visibility === 'visible' && report.result.disposed === true, 'Browser page was hidden or not disposed')
  ensure(report.result.audioAvailable === false && report.result.audioProfile === 'unavailable-real-closed-context',
    'Story baseline audio profile differs from the original workload')
  ensure(Array.isArray(report.result.samples) && report.result.samples.length === 3, 'Exactly three browser samples required')
  for (const sample of [report.result.warmup, ...report.result.samples]) {
    const match = /^DONE:(\d+):(\d+)$/.exec(String(sample?.out))
    ensure(match && sample.out === 'DONE:339:193' && Number(match[1]) === sample.tokens, 'Browser story did not complete the same route')
    ensure(Number.isFinite(sample.wallMs) && sample.wallMs > 0, 'Invalid browser duration')
    ensure(Number.isSafeInteger(sample.frames) && sample.frames > 100, 'Missing real scheduler ticks')
    ensure(Number.isSafeInteger(sample.renderedFrames) && sample.renderedFrames >= 150, 'Missing required real renderer progress')
    ensure(Array.isArray(sample.errors) && sample.errors.length === 0, 'Browser story errors')
    ensure(sample.tokensPerMs === sample.tokens / sample.wallMs && sample.framesPerMs === sample.frames / sample.wallMs,
      'Browser rates differ from raw measurements')
    ensure(Number.isFinite(sample.memGrowthKB), 'Invalid browser heap observation')
    ensure(sample.audioBefore === 'closed' && sample.audioAfter === 'closed'
      && sample.audioContextRetained === true && sample.audioAvailable === false, 'Audio capability changed during measurement')
  }
  return report.result.samples
}

function sourceManifest() {
  const files = new Set([join(web,'package-lock.json'), join(web,'scripts-index.json'),join(web,'perf-baseline.test.js'),
    join(root,'demo/example_game/story.ks'), join(root,'demo/caesura.project.json'),
    join(here,'story-browser-benchmark.js'), fileURLToPath(import.meta.url),
    join(web,'node_modules/wasmoon/dist/glue.wasm'), join(web,'node_modules/wasmoon/dist/index.js'), join(web,'node_modules/wasmoon/package.json'),
    join(root,'tests/scripts/run_story_browser_benchmark.py'),join(root,'scripts/validation_process.py')])
  const walk = (dir, accept) => { for (const entry of readdirSync(dir,{withFileTypes:true})) {
    const path = join(dir,entry.name)
    if (entry.isDirectory()) walk(path,accept)
    else if (entry.isFile() && accept(path)) files.add(path)
  }}
  walk(join(root,'scripts'), path => path.endsWith('.lua'))
  walk(join(root,'assets'), () => true)
  for(const entry of readdirSync(web,{withFileTypes:true})) {
    if(entry.isFile() && entry.name.endsWith('.js') && !entry.name.endsWith('.test.js')) files.add(join(web,entry.name))
  }
  return Object.fromEntries([...files].sort().map(file => [relative(root,file).replaceAll('\\','/'),hash(readFileSync(file))]))
}

class Cdp {
  constructor(socket, observe=()=>{}) {
    this.socket=socket; this.next=0; this.pending=new Map()
    socket.onmessage=event=>{const value=JSON.parse(event.data);if(value.method){observe(value.method,value.params);return}const call=this.pending.get(value.id);if(!call)return
      this.pending.delete(value.id);clearTimeout(call.timer);value.error?call.reject(Error(JSON.stringify(value.error))):call.resolve(value.result)}
  }
  static async connect(url, observe) {
    const socket=new WebSocket(url)
    await new Promise((yes,no)=>{const timer=setTimeout(()=>{socket.close();no(Error('CDP connection deadline'))},5000)
      socket.onopen=()=>{clearTimeout(timer);yes()};socket.onerror=()=>{clearTimeout(timer);no(Error('CDP socket failure'))}})
    return new Cdp(socket, observe)
  }
  send(method, params={}) {
    const id=++this.next
    return new Promise((resolveCall,reject)=>{const timer=setTimeout(()=>{this.pending.delete(id);reject(Error(method+' deadline'))},5000)
      this.pending.set(id,{resolve:resolveCall,reject,timer});this.socket.send(JSON.stringify({id,method,params}))})
  }
  close(){for(const call of this.pending.values()){clearTimeout(call.timer);call.reject(Error('CDP closed'))}this.pending.clear();this.socket.close()}
}

async function run() {
  const out=mkdtempSync(join(tmpdir(),'caesura-story-browser-'))
  const profile=join(out,'profile');mkdirSync(profile)
  const cacheRoot=join(out,'vite-cache');mkdirSync(cacheRoot)
  let server, child, browserCdp, page, endpoint, browserPid, launchError
  let report={passed:false,output:out}, sourceBefore
  const diagnostics=[]
  const diagnostic = (kind, detail) => { if(diagnostics.length<64)diagnostics.push({kind,detail:JSON.stringify(detail).slice(0,2048)}) }
  let bootstrapFailure
  const get = url => fetch(url,{signal:AbortSignal.timeout(1000)})
  try {
    sourceBefore=sourceManifest()
    let browser
    const choices=process.env.CAESURA_TEST_BROWSER?[process.env.CAESURA_TEST_BROWSER]:process.platform==='win32'?['chrome','edge']:['chrome']
    for(const choice of choices){const found=spawnSync(process.execPath,[join(root,'scripts/web_browser_smoke.mjs'),'--print-browser','--browser',choice],{cwd:root,encoding:'utf8',windowsHide:true,timeout:5000})
      if(found.status===0&&existsSync(found.stdout.trim())){browser=found.stdout.trim();break}}
    ensure(browser,'Required real browser unavailable')
    const {createServer}=await import(pathToFileURL(join(web,'node_modules/vite/dist/node/index.js')).href)
    server=await createServer({configFile:false,root:web,cacheDir:cacheRoot,publicDir:false,logLevel:'error',server:{host:'127.0.0.1',port:0,hmr:false,fs:{strict:true,allow:[web,cacheRoot]}},plugins:[{
      name:'story-benchmark-allowlist',configureServer(vite){vite.middlewares.use((request,response,next)=>{
        try {
          const path=decodeURIComponent(new URL(request.url,'http://local').pathname)
          let file, type='application/octet-stream'
          if(path==='/__story__/'){response.setHeader('content-type','text/html');response.end('<!doctype html><meta charset="utf-8"><script type="module" src="/test-support/story-browser-benchmark.js"></script>');return}
          if(path==='/__story__/story.ks'){file=join(root,'demo/example_game/story.ks');type='text/plain'}
          else if(path==='/__story__/project.json'){file=join(root,'demo/caesura.project.json');type='application/json'}
          else if(path==='/__story__/glue.wasm'){file=join(web,'node_modules/wasmoon/dist/glue.wasm');type='application/wasm'}
          else if(path==='/scripts/index.json'){file=join(web,'scripts-index.json');type='application/json'}
          else if(path.startsWith('/scripts/')||path.startsWith('/assets/')){
            const base=join(root,path.startsWith('/scripts/')?'scripts':'assets');file=resolve(root,'.'+path)
            ensure(inside(base,file)&&inside(base,realpathSync(file)),'Asset path escaped allowlist')
            if(file.endsWith('.lua')||file.endsWith('.json'))type='text/plain'
          }
          if(file){response.setHeader('content-type',type);response.setHeader('cache-control','no-store');response.end(readFileSync(file));return}
          const source = /^\/[\w-]+\.js$/.test(path) && !path.endsWith('.test.js')
          const dependency = /^\/node_modules\/\.vite\/deps\/[\w.-]+\.(js|map)$/.test(path)
          const freshDependency = allowFreshViteDependency(path,cacheRoot)
          if(path==='/test-support/story-browser-benchmark.js'||path==='/@vite/client'||source||dependency||freshDependency){next();return}
          diagnostic('http-denied',{path});response.statusCode=404;response.end('Not allowlisted')
        }catch(error){diagnostic('http-source-error',{path:request.url,error:String(error)});response.statusCode=404;response.end('Unavailable source')}
      })}
    }]})
    await server.listen()
    const address=server.httpServer.address();ensure(address&&typeof address!=='string','Missing Vite listener')
    const browserArgs=['--headless=new','--no-first-run','--no-default-browser-check','--disable-extensions','--mute-audio','--remote-debugging-address=127.0.0.1','--remote-debugging-port=0','--user-data-dir='+profile,'--window-size=1280,720']
    if(process.env.CI)browserArgs.push('--no-sandbox','--disable-dev-shm-usage')
    child=spawn(browser,[...browserArgs,'about:blank'],{windowsHide:true,stdio:'ignore'})
    child.on('error',error=>{launchError=error})
    const deadline=Date.now()+60000, portFile=join(profile,'DevToolsActivePort')
    while(!existsSync(portFile)){if(launchError)throw launchError;ensure(Date.now()<deadline,'Browser startup deadline');await sleep(50)}
    const port=Number(readFileSync(portFile,'utf8').split(/\r?\n/)[0]);ensure(Number.isInteger(port)&&port>0&&port<65536,'Invalid owned browser port')
    endpoint='http://127.0.0.1:'+port
    const version=await(await get(endpoint+'/json/version')).json()
    browserCdp=await Cdp.connect(version.webSocketDebuggerUrl)
    const processes=await browserCdp.send('SystemInfo.getProcessInfo')
    browserPid=processes.processInfo.find(item=>item.type==='browser')?.id
    ensure(Number.isSafeInteger(browserPid)&&browserPid>0,'Missing real browser process identity')
    const targets=await(await get(endpoint+'/json/list')).json()
    page=await Cdp.connect(targets.find(target=>target.type==='page').webSocketDebuggerUrl, (method,params)=>{
      if(method==='Runtime.exceptionThrown'){
        diagnostic(method,params.exceptionDetails);bootstrapFailure=JSON.stringify(params.exceptionDetails).slice(0,2048)
      }else if(method==='Network.responseReceived' && params.response.status>=400){
        diagnostic(method,{type:params.type,status:params.response.status,url:params.response.url})
        if(params.type==='Script'||params.type==='Document')bootstrapFailure=`${params.type} HTTP ${params.response.status}: ${params.response.url}`
      }else if(method==='Network.loadingFailed'){
        diagnostic(method,params)
        if(params.type==='Script'||params.type==='Document')bootstrapFailure=`${params.type} loading failed: ${params.errorText}`
      }
    })
    await page.send('Page.enable');await page.send('Runtime.enable');await page.send('Network.enable');await page.send('Page.bringToFront')
    await page.send('Page.navigate',{url:`http://127.0.0.1:${address.port}/__story__/`})
    let result
    do {ensure(Date.now()<deadline,'Story browser workload deadline')
      const value=await page.send('Runtime.evaluate',{expression:'globalThis.__storyBenchmark || null',returnByValue:true})
      ensure(!value.exceptionDetails,'Browser evaluation failed: '+JSON.stringify(value.exceptionDetails));result=value.result.value
      ensure(!bootstrapFailure,'Browser bootstrap exception: '+bootstrapFailure)
      if(!result?.disposed)await sleep(50)
    }while(!result?.disposed)
    ensure(!result.error,result.error)
    report={...report,passed:true,browser,browserPid,browserVersion:version.Browser,browserArguments:browserArgs,
      browserSha256:hash(readFileSync(browser)),node:process.version,result}
  }catch(error){report={...report,passed:false,error:String(error.stack||error)}}
  finally {
    try{await browserCdp?.send('Browser.close')}catch{}
    page?.close();browserCdp?.close()
    let endpointClosed=!endpoint, browserExited=!browserPid&&!child
    const deadline=Date.now()+10000
    while(Date.now()<deadline){
      if(endpoint&&!endpointClosed){try{await get(endpoint+'/json/version')}catch{endpointClosed=true}}
      if(browserPid){try{process.kill(browserPid,0)}catch(error){if(error.code==='ESRCH')browserExited=true;else throw error}}
      else if(child?.exitCode!==null&&child?.exitCode!==undefined)browserExited=true
      if(endpointClosed&&browserExited&&(!child||child.exitCode!==null))break
      await sleep(50)
    }
    // A failed teardown remains a failure; only the retained launcher is touched.
    if(child&&child.exitCode===null){
      child.kill();report.cleanupFallback=true
      const fallbackDeadline=Date.now()+2000
      while(child.exitCode===null&&Date.now()<fallbackDeadline)await sleep(50)
      // The Python owner bounds and retires any remaining browser descendants.
      // A forced or unfinished local teardown cannot be accepted as PASS.
    }
    await server?.close()
    const sourceAfter=sourceManifest(), sourceStable=JSON.stringify(sourceBefore)===JSON.stringify(sourceAfter)
    report={...report,diagnostics,sourceBefore,sourceAfter,sourceStable,endpointClosed,browserExited,launcherExit:child?.exitCode??null}
    report.passed=report.passed&&sourceStable&&endpointClosed&&browserExited&&child?.exitCode===0&&!report.cleanupFallback
    writeFileSync(join(out,'report.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'})
  }
  if(!report.passed)console.error('Browser benchmark report: '+join(out,'report.json'))
  ensure(report.passed, 'Browser benchmark failed; see '+join(out,'report.json'))
  process.stdout.write(JSON.stringify(report)+'\n')
}

if(process.argv[1] && resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  run().catch(error=>{console.error(error.stack||error);process.exitCode=1})
}
