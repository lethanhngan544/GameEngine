#version 450

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Position;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Normal;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Albedo;


layout(set = 2, binding = 0) uniform UniformBuffer
{
	vec4 position;
	vec4 color;
	float intensity;
	float constant;
	float linear;
	float exponent;
} ubo;

layout (location = 0) in vec2 fsUv;

layout (location = 0) out vec4 outColor;

void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 fragPos = subpassLoad(subpass0Position).rgb;
	vec3 normal = subpassLoad(subpass0Normal).rgb;
	vec4 albedo = subpassLoad(subpass0Albedo);
	

	// Calculate pointLight
	
	vec3 lightVec = ubo.position.xyz - fragPos;
	vec3 lightDir = normalize(lightVec);
	float product = max(dot(normal, lightDir), 0.0);
	vec3 fragcolor  = ubo.color.rgb * product * albedo.rgb;

	//Attenuate
	float distance    = length(lightVec);
    float attenuation = 1.0 / (ubo.constant + ubo.linear * distance + 
  			     ubo.exponent * (distance * distance));    
	fragcolor *= attenuation;

	outColor = vec4(fragcolor, 0.0);
}