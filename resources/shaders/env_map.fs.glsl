#version 460 core

out vec4 o_color;

layout(location = 0) in VS_OUT {
    vec3 local_pos;
} fs_in;

uniform samplerCube env_map;

void main() {
    vec3 env_color = texture(env_map, fs_in.local_pos).rgb;
    o_color = vec4(env_color, 1.0);
}
