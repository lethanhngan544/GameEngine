#version 450

layout (input_attachment_index = 3, set = 1, binding = 0) uniform subpassInput subpass0Depth;

layout(location = 0) in vec3 fsColor;
layout(location = 0) out vec4 outColor;

void main() {
    float depth = subpassLoad(subpass0Depth).r;
    if(gl_FragCoord.z > depth) {
        discard;
    }
    outColor = vec4(fsColor, 1.0);
}