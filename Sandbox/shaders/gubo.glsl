
layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4x4 projection;
	mat4x4 view;
    mat4x4 directionalViewProj[MAX_CSM_COUNT];
    vec3 cameraPos;
} gUBO;
