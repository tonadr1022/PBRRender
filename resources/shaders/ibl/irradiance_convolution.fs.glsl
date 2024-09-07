#version 460 core

out vec4 o_color;

layout(location = 0) in VS_OUT {
    vec3 world_pos;
} fs_in;

uniform samplerCube env_map;

const float PI = 3.14159265359;

void main() {
    vec3 normal = normalize(fs_in.world_pos);
    vec3 irradiance = vec3(0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sample_delta = 0.025;
    float num_samples = 0.0;
    // Approximate the convolution integral with discrete samples across the hemisphere oriented by the normal of the fragment
    for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
            // spherical to cartesian in tangent space
            vec3 tangent_sample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world space
            vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal;
            irradiance += texture(env_map, sample_vec).rgb * cos(theta) * sin(theta);
            num_samples++;
        }
    }
    irradiance = irradiance * PI * (1.0 / float(num_samples));
    o_color = vec4(irradiance, 1.0);
}
