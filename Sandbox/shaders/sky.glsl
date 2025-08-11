#version 450

#include "shaders/gubo.glsl"


layout (location = 0) in vec2 fsUv;

layout (location = 0) out vec4 outColor;

//Define the sky color and ground color
const vec3 skyColor = vec3(0.5, 0.7, 1.0); // Light blue sky color
const vec3 groundColor = vec3(1.0, 1.0, 1.0); // Green ground color


float sphere(vec3 p, vec3 center, float radius) 
{
	return length(p - center) - radius;
}

float box(vec3 p, vec3 center, vec3 size) 
{
	vec3 d = abs(p - center) - size;
	return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

//Palate function
vec3 palette(float t) {
    return .5+.5*cos(6.28318*(t+vec3(.3,.416,.557)));
}

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    return o4.y * d.y + o4.x * (1.0 - d.y);
}

float fbm(vec3 p) {
  vec3 q = p + 0.0 * 0.5 * vec3(1.0, -0.2, -1.0);
  float g = noise(q);

  float f = 0.0;
  float scale = 0.5;
  float factor = 2.02;

  for (int i = 0; i < 6; i++) {
      f += scale * noise(q);
      q *= factor;
      factor += 0.21;
      scale *= 0.5;
  }

  return f;
}


float map(vec3 p)
{
	float distance = box(p, vec3(0.0, 1.0, 0.0), vec3(2, 0.5, 2)); // Sphere at origin with radius 1
	float f = fbm(p);
	return -distance + f;
}

vec3 skyDome(vec3 rd) 
{
	float t = 1.0 - rd.y; // Assuming rd is normalized and y is up
	return mix(skyColor, groundColor, t);
}


const int MAX_STEPS = 5;
const float MARCH_SIZE = 0.2;
const vec3 SUN_DIRECTION = normalize(vec3(0.0, -1.0, 0.0)); // Direction of the sun

void main() 
{
	vec2 uv = fsUv * 2.0 - 1.0; // Convert to NDC coordinates
	uv.y = -uv.y; // Flip the y-coordinate

	//vec3 ro = vec3(gUBO.cameraPos * 0.01);
	vec3 ro = vec3(0, 0, 0);
	vec3 rd = normalize(vec3(uv.x * (16.0 / 9.0), uv.y, -1));
	//Rotate rd based on gUBO view matrix
	rd = normalize(vec3(inverse(gUBO.view) * vec4(rd, 0.0)));

	vec4 rayMarchColor = vec4(0);

	vec3 sunDirection = normalize(SUN_DIRECTION);
	float sun = clamp(dot(sunDirection, rd), 0.0, 1.0 );
	vec3 skyColor = vec3(0.7,0.7,0.90);
	skyColor -= 0.8 * vec3(0.90,0.75,0.90) * rd.y;
	skyColor += 0.5 * vec3(1.0,0.5,0.3) * pow(sun, 10.0);

	float t = 0.0;

	for(int i = 0; i < MAX_STEPS; i++) 
	{
		vec3 p = ro + rd * t;
		float d = map(p);
		t += MARCH_SIZE;

		 if (d > 0.0) {
		  //float diffuse = clamp((map(p) - map(p + 0.3 * SUN_DIRECTION)) / 0.3, 0.0, 1.0 );
		  //vec3 lin = vec3(0.60,0.60,0.75) * 1.1 + 0.8 * vec3(1.0,0.6,0.3) * diffuse;
		  vec4 color = vec4(mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), d), d );
		  color.rgb *= color.a;
		  rayMarchColor += color*(1.0-color.a);
		}
	}

	skyColor = skyColor * (1.0 - rayMarchColor.a) + rayMarchColor.rgb;

	outColor =  vec4(skyColor, 1.0);
}