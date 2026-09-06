import {readAssetBytes} from './restore-assets.js'

const DEFAULT_FONT={version:1,active:true,font:2,path:'assets/fonts/NotoSansCJKsc-Regular.otf',size:20}
let familySequence=0

function description(value) {
  if (!value || value.version!==1 || typeof value.active!=='boolean') throw new Error('Invalid font state')
  if (!value.active) return {version:1,active:false}
  const {font,path,size}=value
  if (![0,1,2].includes(font) || !Number.isInteger(size) || size<1 || size>256 || typeof path!=='string') throw new Error('Invalid font selection')
  if (font!==2 && (path!=='' || size!==(font===0?16:32))) throw new Error('Invalid bitmap font preset')
  return {version:1,active:true,font,path,size}
}

/** FontFace decodes owned bytes before its face enters the document font set.
 * Browser glyphs/bitmap presets are host-specific; this is a same-host restore
 * contract, not a claim that DOM and native glyph rasterization are identical. */
export function createFontRestore({core,fetchImpl=globalThis.fetch,assetUrl=path=>path,
  fontSet=globalThis.document?.fonts,FontFaceClass=globalThis.FontFace}) {
  const tickets=new WeakMap(), pending=new Set()
  let selected={version:1,active:false}, activeFace=null, closed=false, revision=0
  function consume(ticket) {
    const state=tickets.get(ticket)
    if (!state || state.used) throw new Error('Font preparation is unavailable or consumed')
    state.used=true;pending.delete(state)
    const face=state.face;state.face=null
    return {face,value:state.value}
  }
  function clear() {
    selected={version:1,active:false}
    core.font=Object.freeze({...selected,family:'',size:20})
    revision++
    // Stop glyph submission even if the host cannot release its face now.
    // Retain that face until successful deletion so cleanup can be retried.
    if (activeFace) fontSet.delete(activeFace)
    activeFace=null
    return true
  }
  const api={
    capture_font:()=>({...selected}),
    default_font:()=>({...DEFAULT_FONT}),
    async prepare_font(input) {
      if (closed) throw new Error('Font preparation host is closed')
      const value=description(input)
      let face=null
      if (value.active && value.font===2) {
        if (!fontSet || typeof FontFaceClass!=='function') throw new Error('Browser font decoder unavailable')
        const bytes=await readAssetBytes(value.path,{fetchImpl,assetUrl,maxBytes:32*1024*1024})
        face=new FontFaceClass('CaesuraRestoredFont'+(++familySequence),bytes.buffer)
        await face.load()
        if (face.status!=='loaded') throw new Error('Font decoder did not produce a loaded face')
      }
      if (closed) throw new Error('Font preparation host is closed')
      const ticket=Object.freeze({}),state={face,value,used:false}
      tickets.set(ticket,state);pending.add(state)
      return ticket
    },
    apply_font(ticket) {
      const {face,value}=consume(ticket)
      if (closed) throw new Error('Font preparation host is closed')
      if (!value.active) return clear()
      const previous=activeFace
      try {
        if (face) fontSet.add(face)
        if (previous) fontSet.delete(previous)
      } catch(error) {
        if (face) {try {fontSet.delete(face)} catch { /* Preserve original failure. */ }}
        throw error
      }
      activeFace=face;selected=value;revision++
      core.font=Object.freeze({...value,family:face?.family??'monospace'})
      return true
    },
    discard_font(ticket) {
      const state=tickets.get(ticket)
      if (state && !state.used) consume(ticket)
    },
    clear_font:clear,
    async select_font(face='default',size=24) {
      const start=++revision
      let ticket
      try {
        const value=face==='bitmap' || face==='builtin'
          ? {version:1,active:true,font:0,path:'',size:16}
          : {version:1,active:true,font:2,path:face==='' || face==='default'?DEFAULT_FONT.path:face,size:Math.floor(size)}
        ticket=await api.prepare_font(value)
        if (closed || revision!==start) {api.discard_font(ticket);return false}
        return api.apply_font(ticket)
      } catch {
        if (ticket) api.discard_font(ticket)
        return false
      }
    },
    dispose() {
      closed=true
      for (const state of pending) {state.face=null;state.used=true}
      pending.clear()
      return clear()
    },
  }
  return api
}
