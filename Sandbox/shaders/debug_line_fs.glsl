#version 450


layout(location = 0) in vec3 fsColor;
layout(location = 0) out vec4 outColor;

void main() {

    outColor = vec4(fsColor, 0.0);
}