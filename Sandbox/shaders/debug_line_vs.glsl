#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fsColor;

#include "shaders/gubo.glsl"


void main() {
    fsColor = inColor;
    gl_Position = gUBO.projection * gUBO.view * vec4(inPos, 1.0);
}