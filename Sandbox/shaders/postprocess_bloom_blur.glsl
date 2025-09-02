#version 450

layout(location = 0) in vec2 fsUv;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uImage[2];

// Direction: (1,0) = horizontal, (0,1) = vertical
layout(push_constant) uniform BlurPush {
    vec2 direction;   // e.g. (1.0, 0.0) or (0.0, 1.0)
    float radius;     // blur radius in texels
    int   pass;       // 0 = horizontal, 1 = vertical
} params;

const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uImage[params.pass], 0));
    vec4 result = vec4(0.0);
    

    // center
    result += texture(uImage[params.pass], fsUv) * weights[0];

    // neighbors
    for (int i = 1; i < 5; ++i) {
        vec2 offset = params.direction * texelSize * float(i) * params.radius;
        result += texture(uImage[params.pass], fsUv + offset) * weights[i];
        result += texture(uImage[params.pass], fsUv - offset) * weights[i];
    }

    outColor = result;
}