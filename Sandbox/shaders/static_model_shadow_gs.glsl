#version 460 core

#include "shaders/gubo.glsl"
    
layout(triangles, invocations = MAX_CSM_COUNT) in;
layout(triangle_strip, max_vertices = 3) out;

    
void main()
{          
    for (int i = 0; i < MAX_CSM_COUNT; ++i)
    {
        gl_Position = 
            gUBO.directionalViewProj[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}  