#version 450

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4x4 projection;
	mat4x4 view;
    mat4x4 directionalViewProj;
    vec3 cameraPos;
} gUBO;

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec2 gsUv;
layout(location = 1) out vec3 gsNormal;

void main() {
    vec4 v = ps.model * vec4(inPos, 1.0);
    gl_Position = gUBO.projection * gUBO.view * v;
    gsNormal = inNormal;
    gsUv = inUv;
}