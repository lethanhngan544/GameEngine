#version 450

#include "shaders/gubo.glsl"

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;

layout(set = 1, binding = 0) uniform BoneUniform
{
    mat4 matrices[MAX_BONES];
} gBoneUniform;


layout(location = 0) in vec3 inPos;
layout(location = 1) in ivec4 inBoneIds;
layout(location = 2) in vec4 inBoneWeights;


void main() {
    mat4 skinMat = 
      inBoneWeights.x * gBoneUniform.matrices[inBoneIds.x] +
      inBoneWeights.y * gBoneUniform.matrices[inBoneIds.y] +
      inBoneWeights.z * gBoneUniform.matrices[inBoneIds.z] +
      inBoneWeights.w * gBoneUniform.matrices[inBoneIds.w];

    gl_Position = ps.model * skinMat * vec4(inPos, 1.0);
}