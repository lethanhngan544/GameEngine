#version 450

layout(set = 1, binding = 0) uniform sampler2D uSceneTex;

layout(location = 0) in vec2 fsUv;

// Outputs
layout(location = 0) out vec4 outScene;   // attachment 0
layout(location = 1) out vec4 outBloom;   // attachment 1
layout(location = 2) out float outLuminance; // attachment 3

//Push constants
layout(push_constant) uniform BloomPS {
    float threshold;
    float knee;
    float renderScale;
} ps;

void main() {
    vec3 color = texture(uSceneTex, fsUv * ps.renderScale).rgb;  // linear input

    // Pass scene straight through
    outScene = vec4(color, 1.0);

    // Bright-pass threshold

    float lum = max(max(color.r, color.g), color.b);
    float t = max(lum - ps.threshold, 0.0);
    t = (t * t) / (t + ps.knee);

    vec3 bloomColor = color * (t / max(lum, 1e-6));
    outBloom = vec4(bloomColor, 1.0);


    color = texture(uSceneTex, gl_FragCoord.xy / vec2(textureSize(uSceneTex, 0))).rgb;
    outLuminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
}