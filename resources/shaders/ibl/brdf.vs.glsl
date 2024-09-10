#version 460 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoords;

out vec2 TexCoords;

void main() {
    TexCoords = a_texcoords;
    gl_Position = vec4(a_position, 1.0);
}
