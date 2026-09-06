// jsdom has no pixel decoder/canvas. Supply a real Skia decoder for integration
// tests; production still uses browser ImageBitmap and browser canvas APIs.
import {createCanvas,loadImage} from '@napi-rs/canvas'
import {Blob} from 'node:buffer'

export function installCanvasHost() {
  const previous={blob:globalThis.Blob,bitmap:globalThis.createImageBitmap,context:HTMLCanvasElement.prototype.getContext}
  const canvases=new WeakMap()
  globalThis.Blob=Blob
  globalThis.createImageBitmap=async blob=>{
    const decoded=await loadImage(Buffer.from(await blob.arrayBuffer()))
    // Skia Image storage follows JS reachability; its shim has no explicit
    // ImageBitmap.close operation. Dropping the prepared resource releases it.
    decoded.close=()=>{}
    return decoded
  }
  HTMLCanvasElement.prototype.getContext=function(kind,...args) {
    if(kind!=='2d') return previous.context.call(this,kind,...args)
    let canvas=canvases.get(this)
    if(!canvas || canvas.width!==this.width || canvas.height!==this.height) {
      canvas=createCanvas(this.width,this.height);canvases.set(this,canvas)
    }
    return canvas.getContext('2d')
  }
  return ()=>{
    globalThis.Blob=previous.blob
    if(previous.bitmap===undefined) delete globalThis.createImageBitmap
    else globalThis.createImageBitmap=previous.bitmap
    HTMLCanvasElement.prototype.getContext=previous.context
  }
}
