#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


layout(location = 0) in vec2 gsUv[];
layout(location = 1) in vec3 gsNormal[];

layout(location = 0) out vec3 fsNormal;
layout(location = 1) out vec2 fsUv;
layout(location = 2) out mat3 fsTBN;

layout(push_constant) uniform PushConstant
{
    mat4 model;
} ps;




void main()
{
	
	vec3 edge1 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 edge2 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec2 deltaUV1 = gsUv[1] - gsUv[0];
	vec2 deltaUV2 = gsUv[2] - gsUv[0];

	float r = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
	vec3 tangent = vec3((edge1 * deltaUV2.y - edge2 * deltaUV1.y) * r);
	vec3 bitangent = vec3((edge2 * deltaUV1.x - edge1 * deltaUV2.x) * r);
	vec3 T = normalize(vec3(ps.model * vec4(tangent, 0.0f)));
	vec3 B = normalize(vec3(ps.model * vec4(bitangent, 0.0f)));


	for(int i = 0; i < 3; i++)
	{

		gl_Position = gl_in[i].gl_Position;

		fsNormal = gsNormal[i];
		fsUv = gsUv[i];
		fsTBN = mat3(T, B, fsNormal);


		EmitVertex();
	}



}