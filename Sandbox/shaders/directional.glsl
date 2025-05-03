#version 450

layout(set = 0, binding = 0) uniform GlobalUniformBuffer {
    mat4 projection;
    mat4 view;
    vec3 cameraPos;
} gUBO;

layout(input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Normal;
layout(input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Albedo;
layout(input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Mr;
layout(input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput subpass0Depth;

layout(set = 1, binding = 4) uniform UniformBuffer {
    vec3 direction;
    float intensity;
    vec4 color;
} ubo;

layout(location = 0) in vec2 fsUv;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

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
    vec2 mr = subpassLoad(subpass0Mr).rg;
    float metallic = mr.r;
    float roughness = clamp(mr.g, 0.04, 1.0); // minimum roughness to avoid artifacts

    vec3 V = normalize(gUBO.cameraPos - fragPos);
    vec3 L = normalize(-ubo.direction);
    vec3 H = normalize(V + L);

    // Calculate reflectance at normal incidence
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular     = numerator / denominator;

    // kS is energy reflected, kD is energy diffused
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 irradiance = ubo.color.rgb * ubo.intensity;

    vec3 Lo = (kD * albedo / PI + specular) * irradiance * NdotL;

    outColor = vec4(Lo, 1.0);
}
