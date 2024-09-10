#version 460 core
#extension GL_ARB_bindless_texture : enable
#extension GL_NV_gpu_shader5 : enable

layout(location = 0) in VS_OUT {
    mat3 TBN;
    vec3 pos_world_space;
    vec3 normal;
    vec2 tex_coords;
    flat uint material_idx;
} fs_in;

out vec4 o_color;

layout(std140, binding = 0) uniform UBOUniforms {
    mat4 vp_matrix;
    mat4 view_matrix;
    mat4 proj_matrix;
    vec3 view_pos;
};

#define MATERIAL_FLAG_NONE 0 << 0
#define MATERIAL_FLAG_METALLIC_ROUGHNESS 1 << 0
#define MATERIAL_FLAG_OCCLUSION_ROUGHNESS_METALLIC 1 << 1
#define MATERIAL_FLAG_ALPHA_MODE_MASK 1 << 2

#define MAX_LIGHTS 100

struct Material {
    vec4 base_color;
    vec4 emissive_factor;
    vec2 uv_scale;
    vec2 uv_offset;
    uint64_t base_color_handle;
    uint64_t metallic_roughness_handle;
    uint64_t occlusion_handle;
    uint64_t normal_handle;
    uint64_t emissive_handle;
    float metallic_factor;
    float roughness_factor;
    float emissive_strength;
    float alpha_cutoff;
    float uv_rotation;
    uint material_flags;
};

layout(std430, binding = 1) readonly buffer Materials {
    Material materials[];
};

struct PointLight {
    vec3 position;
    float _pad1;
    vec3 color;
    float intensity;
};

layout(binding = 1, std140) uniform PointLights {
    PointLight pointLights[MAX_LIGHTS];
};

uniform bool point_lights_enabled = true;
uniform bool directional_light_enabled = true;
uniform vec3 u_directional_dir;
uniform vec3 u_directional_color;

uniform samplerCube irradiance_map;

vec3 FresnelSchlick(float cosTheta, vec3 F0);
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
float DistributionGGX(vec3 normal, vec3 halfVector, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
const float PI = 3.14159265358979323846;

vec2 CalculateUV(Material mat, vec2 uv) {
    mat2 rotation_mat2 = mat2(
            cos(mat.uv_rotation), -sin(mat.uv_rotation),
            sin(mat.uv_rotation), cos(mat.uv_rotation)
        );
    return rotation_mat2 * uv * mat.uv_scale + mat.uv_offset;
}

void main() {
    Material mat = materials[fs_in.material_idx];
    vec2 uv = CalculateUV(mat, fs_in.tex_coords);
    vec4 base_color = mat.base_color;
    float roughness = mat.roughness_factor;
    float metallic = mat.metallic_factor;
    float ao = 1.0;
    vec3 normal;

    if (mat.base_color_handle != 0) {
        base_color = texture(sampler2D(mat.base_color_handle), uv);
        if ((mat.material_flags & MATERIAL_FLAG_ALPHA_MODE_MASK) != 0 && base_color.a < mat.alpha_cutoff) {
            discard;
        }
    }
    if (mat.metallic_roughness_handle != 0 && (mat.material_flags & MATERIAL_FLAG_METALLIC_ROUGHNESS) != 0) {
        vec4 metallic_roughness_tex = texture(sampler2D(mat.metallic_roughness_handle), uv);
        roughness = metallic_roughness_tex.g;
        metallic = metallic_roughness_tex.b;
    } else if (mat.metallic_roughness_handle != 0
            && (mat.material_flags & MATERIAL_FLAG_OCCLUSION_ROUGHNESS_METALLIC) != 0) {
        vec4 occ_rough_metallic_tex = texture(sampler2D(mat.metallic_roughness_handle), uv);
        ao = occ_rough_metallic_tex.r;
        roughness = occ_rough_metallic_tex.g;
        metallic = occ_rough_metallic_tex.b;
    }
    if (mat.occlusion_handle != 0) {
        ao = texture(sampler2D(mat.occlusion_handle), uv).r;
    }
    if (mat.normal_handle != 0) {
        normal = texture(sampler2D(mat.normal_handle), uv).rgb;
        // transform to [-1,1]
        normal = normal * 2.0 - 1.0;
        // apply TBN matrix: tangent space -> world space
        normal = normalize(fs_in.TBN * normal);
    } else {
        normal = normalize(fs_in.normal);
    }

    vec3 albedo = base_color.rgb;
    vec3 V = normalize(view_pos - fs_in.pos_world_space);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 light_out = vec3(0.0);
    if (point_lights_enabled) {
        for (int i = 0; i < 1; i++) {
            vec3 L = normalize(pointLights[i].position.xyz - fs_in.pos_world_space);
            vec3 H = normalize(V + L);
            float dist_to_light = length(pointLights[i].position.xyz - fs_in.pos_world_space);
            float attenuation = 1.0 / (dist_to_light * dist_to_light);
            vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation;
            float NDF = DistributionGGX(normal, H, roughness);
            float G = GeometrySmith(normal, V, L, roughness);
            vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
            vec3 specular = numerator / denominator;
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;
            float NdotL = max(dot(normal, L), 0.0);
            light_out += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
        }
    }

    if (directional_light_enabled) {
        vec3 L = normalize(-u_directional_dir);
        vec3 H = normalize(V + L);
        vec3 radiance = u_directional_color;
        float NDF = DistributionGGX(normal, H, roughness);
        float G = GeometrySmith(normal, V, L, roughness);
        vec3 F = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(normal, V), 0.0) * max(dot(normal, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        // scale light by NdotL
        float NdotL = max(dot(normal, L), 0.0);
        vec3 directional_out = ((kD * albedo.rgb / PI + specular) * radiance * NdotL);
        light_out += directional_out;
    }

    vec3 emissive;
    if (mat.emissive_handle != 0) {
        emissive = texture(sampler2D(mat.emissive_handle), uv).rgb * mat.emissive_strength;
    } else {
        emissive = mat.emissive_factor.rgb * mat.emissive_strength;
    }

    vec3 kS = FresnelSchlickRoughness(clamp(dot(normal, V), 0.0, 1.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    vec3 irradiance = texture(irradiance_map, normal).rgb;
    vec3 diffuse = irradiance * albedo;
    vec3 ambient = (kD * diffuse);

    vec3 color = light_out + emissive + ambient;
    // color = color / (color + vec3(1.0));
    // color = pow(color, vec3(1.0 / 2.2));
    o_color = vec4(color, base_color.a);
}

// F0: surface reflection at zero incidence - how much surface reflects if looking directly at the surface.
// varies per material. tinted on metals. Common practice is to use 0.04 for dielectrics
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// D in the Cook-Torrance BRDF: normal Distribution function.
// approximates the amount of the surface's microfacets that are aligned to the halfway vector, influenced by
// surface roughness
// This is the Trowbridge-Reitz GGX.
// when roughness is low, a highly concentrated number of microfacets are aligned to halfway vectors over a small radius
// causing a bright spot and darker over the other vectors. On a rough surface, microfacets are aligned in more
// random directions, so a much large number of halfway vectors are relatively aligned to the microfacets, but less concentrated,
// resulting in a more diffuse appearance
// roughness is used here because term specific modifications can be made.
float DistributionGGX(vec3 normal, vec3 halfVector, float roughness) {
    // Disney and Epic Games observed that squaring the roughness in the geometry and normal distribution function is the best
    // value for a.
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(normal, halfVector), 0.0);
    float NdotH2 = NdotH * NdotH;
    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    denominator = PI * denominator * denominator;
    return numerator / denominator;
}

// approximates relative surface area where its micro surface details overshadow eachother, causing light rays to be
// excluded. 1.0 measures no shadowing, and 0.0 measures complete shadowing. Intuitively, there is more shadowing
// when cos(theta) is closer to 0 compared to looking at a surface straight on.
float GeometrySchlickGGX(float NdotV, float roughness) {
    // remap roughness for direct lighting. Why? Beyond me at this point
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    // rougher surfaces have a higher probability of overshadowing microfacets
    float numerator = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
    return numerator / denominator;
}

// to approximate geometry, need to take view direction (geometry obstruction) and light direction (geometry shadowing)
// into account by multiplying the two calculations.
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    return ggx1 * ggx2;
}
