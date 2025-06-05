#version 450

layout(set = 0, binding = 0) uniform GlobalUniformBuffer {
    mat4 projection;
    mat4 view;
    mat4x4 directionalViewProj;
    vec3 cameraPos;
} gUBO;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Normal;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Albedo;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Mr;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput subpass0Depth;
layout(set = 1, binding = 4) uniform sampler2D uDepthMap;

layout(set = 2, binding = 0) uniform UniformBuffer {
    vec3 direction;
    float intensity;
    vec4 color;
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
    vec3 L = normalize(-ubo.direction);
    vec3 R = reflect(-L, N);

    float shininess = 64.0;
    vec3 lightColor = ubo.color.rgb * ubo.intensity;

    // Phong shading
    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(R, V), 0.0), shininess);

    vec3 ambient = 0.1 * albedo;
    vec3 diffuse = diff * albedo * lightColor;
    vec3 specular = spec * lightColor;

    // Shadow map
    vec4 fragPosLightSpace = gUBO.directionalViewProj * vec4(fragPos, 1.0);
    vec3 shadowCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5; 
    float closestDepth = texture(uDepthMap, shadowCoord.xy).r;   
    float currentDepth = shadowCoord.z;  
    float shadow = currentDepth < closestDepth + 0.0001 ? 1.0 : 0.0; 

    vec3 lighting = ambient + shadow * (diffuse + specular);
    outColor = vec4(lighting, 1.0);
}
