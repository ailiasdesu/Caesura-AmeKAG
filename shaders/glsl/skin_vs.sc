#version 430 core
uniform vec4 u_skinDraw;  // x, y, scale, opacity
uniform vec4 u_skinView;  // width, height

in vec2 a_position;
in vec2 a_texcoord0;
out vec2 v_texcoord0;

void main()
{
    vec2 p = vec2(u_skinDraw.x + a_position.x * u_skinDraw.z,
                  u_skinDraw.y + a_position.y * u_skinDraw.z);
    gl_Position = vec4(p.x / u_skinView.x * 2.0 - 1.0,
                       1.0 - p.y / u_skinView.y * 2.0, 0.0, 1.0);
    v_texcoord0 = a_texcoord0;
}
