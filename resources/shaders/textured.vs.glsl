#version 460 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_tex_coords;

layout(location = 0) out VS_OUT {
    vec2 tex_coords;
    flat uint material_idx;
} vs_out;

struct UniformData {
    mat4 model;
    uint material_index;
};

layout(std430, binding = 0) readonly buffer Uniforms {
    UniformData uniforms[];
};

uniform mat4 u_vp_matrix;

void main() {
    UniformData uniform_data = uniforms[gl_InstanceID + gl_BaseInstance];
    vs_out.tex_coords = a_tex_coords;
    vs_out.material_idx = uniform_data.material_index;
    gl_Position = u_vp_matrix * uniform_data.model * vec4(a_position, 1.0);
}
