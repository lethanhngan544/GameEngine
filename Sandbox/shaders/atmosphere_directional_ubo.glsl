struct AtmosphereDirectionalUBO {
    vec3 direction;
    float intensity;
    vec4 color;
    mat4x4 csmMatrices[MAX_CSM_COUNT];
	float csmPlanes[MAX_CSM_COUNT];
};