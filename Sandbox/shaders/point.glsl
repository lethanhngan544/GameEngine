#version 450

const float PI = 3.14159265359;

layout(set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4x4 projection;
	mat4x4 view;
    vec3 cameraPos;
} gUBO;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput subpass0Normal;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput subpass0Albedo;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput subpass0Mr;
layout (input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput subpass0Depth;

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

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}



void main() 
{
	// Read G-Buffer values from previous sub pass
	vec3 albedo = subpassLoad(subpass0Albedo).rgb;
	vec3 mr = subpassLoad(subpass0Mr).rgb;
	float roughness = mr.g;
	float metallic = mr.b;
	float ao = 1;

	float depth = subpassLoad(subpass0Depth).r;
	vec4 clipSpacePos = vec4(fsUv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePos = inverse(gUBO.projection) * clipSpacePos;
    viewSpacePos /= viewSpacePos.w; // Perspective divide
	vec4 worldSpacePos = inverse(gUBO.view) * viewSpacePos;
	vec3 fragPos = worldSpacePos.xyz;

	vec3 N = subpassLoad(subpass0Normal).rgb;
	vec3 V = normalize(gUBO.cameraPos - fragPos);


	vec3 Lo = vec3(0.0);
	for(int i = 0; i < 4; i++)
	{
		vec3 L = normalize(ubo.position.xyz  - fragPos);
		vec3 H = normalize(V + L);
		float distance    = length(ubo.position.xyz  - fragPos);

		float attenuation = 1.0 / (distance * distance);
		vec3 radiance     = ubo.color.rgb * attenuation; 

		vec3 F0 = vec3(0.04); 
		F0      = mix(F0, albedo, metallic);
		vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
		float NDF = DistributionGGX(N, H, roughness);       
		float G   = GeometrySmith(N, V, L, roughness);   
		
		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
		vec3 specular     = numerator / denominator; 

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
  
		kD *= 1.0 - metallic;	

		
  
		float NdotL = max(dot(N, L), 0.0);        
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 color   = ambient + Lo;  
	/*color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0/2.2));*/

	outColor = vec4(color, 1.0);
}