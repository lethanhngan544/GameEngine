#version 450

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Position;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Normal;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Albedo;

layout (location = 0) in vec2 fsUv;

layout (location = 0) out vec4 outColor;

void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 fragPos = subpassLoad(subpass0Position).rgb;
	vec3 normal = subpassLoad(subpass0Normal).rgb;
	vec4 albedo = subpassLoad(subpass0Albedo);
	

	// Ambient part
	vec3 fragcolor  = albedo.rgb * 0.5;
	
	outColor = vec4(fragcolor, 1.0);
}