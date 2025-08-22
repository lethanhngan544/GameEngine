#version 450

#include "shaders/gubo.glsl"

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Normal;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Albedo;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Mr;
layout (set = 1, binding = 3) uniform sampler2D subpass0Depth;
layout (set = 1, binding = 4, std140) uniform UniformBuffer
{
	vec4 color;
	vec2 noiseScale;
	float radius;
	float intensity;
	float renderScale;
	vec4 SSAOKernel[SSAO_KERNEL_SIZE];
} ubo;
layout (set = 1, binding = 5) uniform sampler2D ssaoNoise; 

layout (location = 0) in vec2 fsUv;
layout (location = 0) out vec4 outColor;


vec3 getPositionFromDepth(float depth, vec2 uv)
{
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = inverse(gUBO.projection) * clipSpace;
	viewSpace /= viewSpace.w;
	vec4 worldSpace = inverse(gUBO.view) * viewSpace;
	return worldSpace.xyz;
}

void main() 
{
	vec3 fragPos = getPositionFromDepth(texture(subpass0Depth, fsUv * ubo.renderScale).r, fsUv);
	fragPos = vec3(gUBO.view * vec4(fragPos, 1.0));

	vec3 albedo = subpassLoad(subpass0Albedo).rgb;
	vec3 normal = subpassLoad(subpass0Normal).rgb;
	normal = vec3(gUBO.view * vec4(normal, 0.0));

	vec3 randomVec = normalize(texture(ssaoNoise, fsUv * ubo.noiseScale).xyz);
	vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN       = mat3(tangent, bitangent, normal);  

	float occlusion = 0.0;
	for(int i = 0; i < SSAO_KERNEL_SIZE; ++i)
	{
		// get sample position
		vec3 samplePos = TBN * ubo.SSAOKernel[i].xyz; // from tangent to view-space
		samplePos = fragPos + samplePos * ubo.radius; 
    
		vec4 offset = vec4(samplePos, 1.0);
		offset      = gUBO.projection * offset;    // from view to clip-space
		offset.xyz /= offset.w;               // perspective divide
		vec2 sampleUV = offset.xy * 0.5 + 0.5;

		 // discard if off-screen
        if (any(bvec2(sampleUV.x < 0.0 || sampleUV.x > 1.0,
                      sampleUV.y < 0.0 || sampleUV.y > 1.0)))
            continue;

		vec3 fragPosSampled = getPositionFromDepth(texture(subpass0Depth, sampleUV  * ubo.renderScale).r, sampleUV);
		fragPosSampled = vec3(gUBO.view * vec4(fragPosSampled, 1.0));


		float rangeCheck = smoothstep(0.0, 1.0, ubo.radius / abs(fragPos.z - samplePos.z));
		occlusion += (fragPosSampled.z >= samplePos.z ? 1.0 : 0.0) * rangeCheck;  
		
	} 
	occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE);


	// Ambient part
	vec3 fragcolor  = albedo * ubo.color.rgb * ubo.intensity * occlusion;
	
	outColor = vec4(fragcolor, 1.0);
}