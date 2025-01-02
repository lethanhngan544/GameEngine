#version 450

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);


layout(location = 0) in vec3 inPos;

void main() {
    gl_Position = vec4(inPos, 1.0);
}