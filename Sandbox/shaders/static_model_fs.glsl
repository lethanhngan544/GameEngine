#version 450

layout(location = 0) in vec3 fsNormal;
layout(location = 1) in vec2 fsUv;
layout(location = 2) in vec3 fsPosition;
layout(location = 3) in mat3 fsTBN;

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outAlbeo;
layout(location = 3) out vec3 outMr;

layout(set = 1, binding = 0) uniform UniformBuffer
{
    uint has_albedo;
    uint has_normal;
    uint has_mr;
} ubo;
layout(set = 1, binding = 1) uniform sampler2D uAlbedo;
layout(set = 1, binding = 2) uniform sampler2D uNormal;
layout(set = 1, binding = 3) uniform sampler2D uMr;



void main() {
    vec3 albedo = vec3(1.0);
    vec3 mr = vec3(1.0);
    vec3 normal = normalize(fsNormal);

    if(ubo.has_normal == 1)
    {
        vec3 normalMap = texture(uNormal, fsUv).rgb * 2.0 - 1.0;
        normal = normalize(fsTBN * normalMap);
    }

    if(ubo.has_albedo == 1)
    {
        vec4 sampledAlbedo =  texture(uAlbedo, fsUv);
        albedo = sampledAlbedo.rgb;
        if(sampledAlbedo.a < 0.1)
        {
            discard;
        }
    }

    if(ubo.has_mr == 1)
    {
        mr = texture(uMr, fsUv).rgb;
    }

    outAlbeo = albedo;
    outNormal = normal;
    outPosition = fsPosition;
    outMr = mr;
}