#version 450

#include "shaders/gubo.glsl"

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;


layout(set = 2, binding = 0) uniform BoneUniform
{
    mat4 matrices[MAX_BONES];
} gBoneUniform;


layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in ivec4 inBoneIds;
layout(location = 4) in vec4 inBoneWeights;

layout(location = 0) out vec2 gsUv;
layout(location = 1) out vec3 gsNormal;


void main() {

    mat4 skinMat = 
      inBoneWeights.x * gBoneUniform.matrices[inBoneIds.x] +
      inBoneWeights.y * gBoneUniform.matrices[inBoneIds.y] +
      inBoneWeights.z * gBoneUniform.matrices[inBoneIds.z] +
      inBoneWeights.w * gBoneUniform.matrices[inBoneIds.w];

    gl_Position = gUBO.projection * gUBO.view * ps.model * skinMat * vec4(inPos, 1.0);
    gsNormal = vec3(ps.model * vec4(inNormal, 0.0f));
    gsUv = inUv;


    
}