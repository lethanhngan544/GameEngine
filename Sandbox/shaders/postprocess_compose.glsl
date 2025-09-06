#version 450

#include "shaders/gubo.glsl"

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput uScene;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput uBloom;

layout (location = 0) out vec4 outColor;

//Push constant
layout (push_constant) uniform PushConstants {
    float exposure;
    float saturation;
    float gamma;
} ps;

void main()
{
    // Load from input attachments
    vec3 sceneColor = subpassLoad(uScene).rgb;
    vec3 bloomColor = subpassLoad(uBloom).rgb;

    // Simple additive blending
    vec3 result = sceneColor + bloomColor; // bloom intensity

    

    // Tone mapping (Reinhard)
    // exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-result * ps.exposure);
    //Gamma correction
    mapped = pow(mapped, vec3(1.0 / ps.gamma));
    //Saturation
    float grey = dot(mapped, vec3(0.299, 0.587, 0.114));
    mapped = mix(vec3(grey), mapped, ps.saturation);
   
    // Output final color
    
    outColor = vec4(mapped, 1.0);
}