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


void main() {
    vec4 v = ps.model * vec4(inPos, 1.0);
    gl_Position = gUBO.directionalViewProj * v;
}