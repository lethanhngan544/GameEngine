#version 430 core

#include "shaders/gubo.glsl"

layout(location = 0) in vec2 fsUv;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uDefaultRenderPassDrawImage; // scene color
layout(set = 1, binding = 1) uniform sampler2D uDepthImage;
layout(set = 1, binding = 2) uniform sampler2D uNormalImage;
layout(set = 1, binding = 3) uniform sampler2D uMr;

// Push constants
layout(push_constant) uniform PushConstants { 
    float renderScale;
    int scaledWidth;
    int scaledHeight;
    float resolution;
    int steps;
    float thickness;
} ps;


// Reconstruct view-space position from depth
vec3 fragPosViewFromDepth(float depth, vec2 uv)
{
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0); 
    vec4 viewSpace = inverse(gUBO.projection) * clipSpace;
    viewSpace /= viewSpace.w;
    return viewSpace.xyz;
}

// Screen-space raymarch for SSR
vec3 rayMarchSSR(vec3 originVS, vec3 dirVS, out bool hit)
{
    hit = false;
    vec3 marchPos = originVS;

    for (int i = 0; i < ps.steps; i++) {
        marchPos += dirVS * ps.resolution;

        // Project back to screen
        vec4 clip = gUBO.projection * vec4(marchPos, 1.0);
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;

        // Discard if outside screen
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            break;

        uv *= ps.renderScale;

        vec3 scenePos = fragPosViewFromDepth(texture(uDepthImage, uv).r, uv);

        // Depth check (thickness tolerance)
        if (scenePos.z > (marchPos.z - ps.thickness)) {
            // Found intersection -> binary refine
            vec3 hi = marchPos;              // current hit
            vec3 lo = marchPos - dirVS * ps.resolution; // last step before hit

            vec2 refinedUV = uv;

            for (int j = 0; j < 5; j++) { // 5 iterations is enough
                vec3 mid = (lo + hi) * 0.5;
                vec4 midClip = gUBO.projection * vec4(mid, 1.0);
                vec3 midNDC = midClip.xyz / midClip.w;
                vec2 midUV = midNDC.xy * 0.5 + 0.5;
                midUV *= ps.renderScale;
                vec3 midScenePos = fragPosViewFromDepth(texture(uDepthImage, midUV).r, midUV);

                if (midScenePos.z > (mid.z - ps.thickness))
                {
                    hi = mid; // still inside, move closer
                    refinedUV = midUV; // update UV
                }
                else
                    lo = mid; // outside, move back
            }

            hit = true;
            return texture(uDefaultRenderPassDrawImage, refinedUV).rgb;
        }
    }

    return vec3(0.0);
}

void main() {
    vec2 uv = fsUv * ps.renderScale;

    // Base scene color
    vec3 baseCol = texture(uDefaultRenderPassDrawImage, uv).rgb; 

    // Get depth
    float depth = texture(uDepthImage, uv).r;

    // Reconstruct normal and position
    vec3 normalVS = texture(uNormalImage, uv).rgb;
    normalVS = normalize(mat3(gUBO.view) * normalVS);

    vec3 fragPosVS = fragPosViewFromDepth(depth, uv);
    vec3 V = normalize(fragPosVS);
    vec3 R = normalize(reflect(V, normalVS));

    // Raymarch reflection
    bool hit;
    vec3 reflCol = rayMarchSSR(fragPosVS, R, hit);

    // Material info
    vec3 mr = texture(uMr, uv).rgb;
    float roughness = mr.g;
    float metalness = mr.b;

    float blend = (hit ? 0.5 : 0.0);
    
    //Reflection strength: sharp for smooth surfaces, weak for rough
    float reflStrength = (1.0 - roughness) * blend;

    // For dielectrics: reflection only small fraction
    float dielectricMix = (1.0 - metalness) * 0.04; // ~F0 for plastics

    // Metals: reflection dominates
    float metalMix = metalness;

    // Final reflection weight
    float reflWeight = reflStrength * (dielectricMix + metalMix);

    // Metals tint reflection with base color (Fresnel approx)
    vec3 tint = mix(vec3(1.0), baseCol, metalness);
    vec3 finalRefl = reflCol * tint;

    // Blend base and reflection
    vec3 finalColor = mix(baseCol, finalRefl, reflWeight);

    outColor = vec4(finalColor, 1.0);
}
