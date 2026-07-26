local layers  = require("layers")
local backend = require("backend")

function engine_update(dt) end

function engine_render()
    layers.init()
    local root = layers.get_root()
    local r, g, b, a = 255, 0, 0, 255
    root.texture = backend.create_solid_texture(r, g, b, a)
    layers.set_layer_image(root, root.texture)
    layers.render()
end
