#version 460 core

layout(location = 0) in VS_OUT {
    vec3 local_pos;
} fs_in;

uniform sampler2D equirectangular_map;

out vec4 o_color;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v) {
    // azimuth and elevation angles
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    // divide by 2*PI and PI to scale into the range [-0.5,0.5]
    uv *= invAtan;
    // transform to [0,1]
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(fs_in.local_pos));
    o_color = vec4(texture(equirectangular_map, uv).rgb, 1.0);
}
