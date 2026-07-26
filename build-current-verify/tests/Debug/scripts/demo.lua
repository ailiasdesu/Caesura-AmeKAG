local layers  = require("layers")
local backend = require("backend")
local w, h = backend.get_resolution()
if not w then w, h = 1280, 720 end
local C = {
    bgDark = {200,20,20,255},
    accent = {20,200,20,255},
}
local function solid(r,g,b,a) return backend.create_solid_texture(math.floor(r),math.floor(g),math.floor(b),math.floor(a or 255)) end
local scene = { frame=0 }
function scene_init()
    layers.init()
    scene.bg = layers.get_root()
    scene.bg.texture = solid(C.bgDark[1],C.bgDark[2],C.bgDark[3],255)
    layers.set_layer_image(scene.bg, scene.bg.texture)
    local tl = layers.add_layer(scene.bg, {name="top",z=1,x=0,y=0,w=w,h=20,visible=true})
    tl.texture = solid(C.accent[1],C.accent[2],C.accent[3],255)
    layers.set_layer_image(tl, tl.texture)
    local bl = layers.add_layer(scene.bg, {name="bottom",z=1,x=0,y=h-20,w=w,h=20,visible=true})
    bl.texture = solid(20,20,200,255)
    layers.set_layer_image(bl, bl.texture)
end
function engine_update(dt)
    if not scene.bg then scene_init() end
    scene.frame = scene.frame + 1
end
function engine_render() layers.render() end
