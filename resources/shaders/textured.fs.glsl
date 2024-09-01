#version 460 core
#extension GL_ARB_bindless_texture : enable
#extension GL_NV_gpu_shader5 : enable

layout(location = 0) in VS_OUT {
    vec2 tex_coords;
    flat uint material_idx;
} fs_in;

out vec4 o_color;

struct Material {
    // vec4 base_color;
    uint64_t tex_handle;
    // float alpha_cutoff;
    // TODO: see if can eliminate
    // vec3 pad;
};

layout(std430, binding = 1) readonly buffer Materials {
    Material materials[];
};

void main() {
    Material mat = materials[fs_in.material_idx];
    // const bool has_base_tex = (mat.tex_handle.x != 0 || mat.tex_handle.y != 0);
    const bool has_base_tex = mat.tex_handle != 0;
    vec4 base_color_tex = vec4(1);
    if (has_base_tex) {
        base_color_tex = texture(sampler2D(mat.tex_handle), fs_in.tex_coords);
        // if (base_color_tex.a < mat.alpha_cutoff) {
        //     discard;
        // }
    }
    o_color = base_color_tex;
    // o_color = base_color_tex * mat.base_color;
}
