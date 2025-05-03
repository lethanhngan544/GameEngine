#version 450

layout(location = 0) in vec2 inVertex;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inPositionSize; 
layout(location = 3) in uvec2 inFrameIndex; 

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4x4 projection;
	mat4x4 view;
    vec3 cameraPos;
} gUBO;


layout(location = 0) out vec2 fsUv;

//Push constant
layout(push_constant) uniform PushConstant
{
    ivec2 atlasSize;
} ps;

void main() {
    vec3 pos = inPositionSize.xyz;
    float size = inPositionSize.w;

    // Extract right and up from the view matrix (column-major layout)
    vec3 right = vec3(gUBO.view[0][0], gUBO.view[1][0], gUBO.view[2][0]);
    vec3 up    = vec3(gUBO.view[0][1], gUBO.view[1][1], gUBO.view[2][1]);

    vec3 offset = (inVertex.x * right + inVertex.y * up) * size;
    vec4 worldPos = vec4(pos + offset, 1.0);

    gl_Position = gUBO.projection * gUBO.view * worldPos;

    //Caculate uv
    vec2 frameOffset = vec2(inFrameIndex) / vec2(ps.atlasSize);
    vec2 frameScale = vec2(1.0) / vec2(ps.atlasSize);

    fsUv = frameOffset + inUv * frameScale;
}