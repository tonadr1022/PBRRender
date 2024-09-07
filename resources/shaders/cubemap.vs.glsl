#version 460 core

layout(location = 0) in vec3 a_position;

layout(location = 0) out VS_OUT {
    vec3 world_pos;
} vs_out;

uniform mat4 u_projection;
uniform mat4 u_view;

void main() {
    vs_out.world_pos = a_position;
    gl_Position = u_projection * u_view * vec4(a_position, 1.0);
}
