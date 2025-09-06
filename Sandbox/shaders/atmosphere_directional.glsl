#version 450

#include "shaders/gubo.glsl"
#include "shaders/atmosphere_directional_ubo.glsl"

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Normal;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Albedo;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Mr;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput subpass0Depth;
layout(set = 1, binding = 4) uniform sampler2DArray uDepthMap;
layout(set = 1, binding = 5) uniform UniformBuffer {
    AtmosphereDirectionalUBO ubo;
} ubo;

layout(location = 0) in vec2 fsUv;
layout(location = 0) out vec4 outColor;

void main() {
    // Reconstruct world position
    float depth = subpassLoad(subpass0Depth).r;
    vec4 clipSpace = vec4(fsUv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpace = inverse(gUBO.projection) * clipSpace;
    viewSpace /= viewSpace.w;
    vec4 worldSpace = inverse(gUBO.view) * viewSpace;
    vec3 fragPos = worldSpace.xyz;

    // G-buffer
    vec3 N = normalize(subpassLoad(subpass0Normal).xyz);
    vec3 albedo = subpassLoad(subpass0Albedo).rgb;

    vec3 V = normalize(gUBO.cameraPos - fragPos);
    vec3 L = normalize(-ubo.ubo.direction);
    vec3 R = reflect(-L, N);

    float shininess = 64.0;
    vec3 lightColor = ubo.ubo.color.rgb * ubo.ubo.intensity;

    // Phong shading
    float diff = max(dot(N, L), 0.0);
    float dotProduct = max(dot(R, V), 0.0);
    float spec = pow(dotProduct, shininess);

    vec3 ambient = 0.1 * albedo;
    vec3 diffuse = diff * albedo * lightColor;
    vec3 specular = spec * lightColor;

    float viewSpaceDepth = abs(viewSpace.z);
    //Select layer based on depth
    int layer = 0;
    for (int i = 0; i < MAX_CSM_COUNT; i++) {
        if (viewSpaceDepth < (ubo.ubo.csmPlanes[i] - 2)) {
            layer = i;
            break;
        }
    }
   

    vec4 fragPosLightSpace = ubo.ubo.csmMatrices[layer] * vec4(fragPos, 1.0);
    vec3 shadowCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5; 
    float currentDepth = shadowCoord.z;  

    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);  
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uDepthMap, 0));
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(uDepthMap, vec3(shadowCoord.xy + vec2(x, y) * texelSize, layer)).r; 
            shadow += currentDepth > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;


    vec3 lighting = ambient + (1.0f - shadow) * (diffuse + specular);
    outColor = vec4(lighting, 1.0);
}
