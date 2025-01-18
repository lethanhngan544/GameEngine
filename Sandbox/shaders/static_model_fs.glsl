#version 450

layout(location = 0) in vec3 fsNormal;
layout(location = 1) in vec2 fsUv;
layout(location = 2) in vec3 fsPosition;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbeo;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;


void main() {
    outAlbeo = texture(uAlbedo, fsUv);
    outNormal = vec4(normalize(fsNormal), 0);
    outPosition = vec4(fsPosition, 0);
}