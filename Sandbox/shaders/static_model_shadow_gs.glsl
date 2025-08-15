#version 460 core

#include "shaders/gubo.glsl"
#include "shaders/atmosphere_directional_ubo.glsl"
    
layout(triangles, invocations = MAX_CSM_COUNT) in;
layout(triangle_strip, max_vertices = 3) out;

layout(set = 1, binding = 5, std140) uniform TheBlock
{
    AtmosphereDirectionalUBO ubo;
} ubo;

    
void main()
{          
    for (int i = 0; i < MAX_CSM_COUNT; ++i)
    {
        gl_Position = 
           ubo.ubo.csmMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}  