#version 460 core

layout(location = 0) in vec3 a_position;

layout(location = 0) out VS_OUT {
    vec3 local_pos;
} vs_out;

layout(std140, binding = 0) uniform UBOUniforms {
    mat4 vp_matrix;
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 view_pos;
};

void main() {
    vs_out.local_pos = a_position;
    vec4 clip_pos = proj_matrix * mat4(mat3(view_matrix)) * vec4(a_position, 1.0);
    // always make depth of 1 to render skybox
    gl_Position = clip_pos.xyww;
}
