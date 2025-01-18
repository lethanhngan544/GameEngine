#version 450

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4x4 projection;
	mat4x4 view;
} gUBO;

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 fsNormal;
layout(location = 1) out vec2 fsUv;
layout(location = 2) out vec3 fsPosition;

void main() {
    vec4 v = ps.model * vec4(inPos, 1.0);
    gl_Position = gUBO.projection * gUBO.view * v;
    fsPosition = v.xyz;
    fsNormal = (mat3(ps.model) * inNormal).xyz;
    fsUv = inUv;
}