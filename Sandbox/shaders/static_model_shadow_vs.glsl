#version 450

#include "shaders/gubo.glsl"

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;


layout(location = 0) in vec3 inPos;


void main() {
    gl_Position = ps.model * vec4(inPos, 1.0);
}