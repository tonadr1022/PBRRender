#version 460 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec2 a_tex_coords;

layout(location = 0) out VS_OUT {
    mat3 TBN;
    vec3 pos_world_space;
    vec3 normal;
    vec2 tex_coords;
    flat uint material_idx;
} vs_out;

struct UniformData {
    mat4 model;
    mat4 normal_matrix;
    uint material_index;
};

layout(std140, binding = 0) uniform UBOUniforms {
    mat4 vp_matrix;
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 view_pos;
};

layout(std430, binding = 0) readonly buffer Uniforms {
    UniformData uniforms[];
};

void main() {
    UniformData uniform_data = uniforms[gl_InstanceID + gl_BaseInstance];
    vs_out.tex_coords = a_tex_coords;
    vs_out.material_idx = uniform_data.material_index;
    vs_out.normal = mat3(uniform_data.normal_matrix) * normalize(a_normal);
    vec4 pos_world_space = uniform_data.model * vec4(a_position, 1.0);
    gl_Position = vp_matrix * pos_world_space;
    vs_out.pos_world_space = vec3(pos_world_space);
    // Gram-Schmidt process to calculate bitangent vector
    vec3 tangent = normalize(vec3(uniform_data.model * vec4(a_tangent, 0.0)));
    vec3 normal = normalize(vec3(uniform_data.model * vec4(a_normal, 0.0)));
    // subtract projection of tangent onto normal to make it orthogonal to normal
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    // bitangent is orthogonal to tangent and normal
    vec3 bitangent = cross(tangent, normal);
    vs_out.TBN = mat3(tangent, bitangent, normal);
}
