#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

#define M_PI 3.14159265358979323846264338327950288

// global resources

layout(set=0, binding=0)
uniform SceneConstants
{
	mat4 matView;
	mat4 matProj;
	mat4 matViewProj;
	mat4 matViewProjInv;
	vec4 cameraPosition;
	ivec2 outputSize;
	uint frameIndex;
	uint padding;
};

layout(set=0, binding=1)
uniform sampler defaultSampler;

layout(set=0, binding=2, rgba32f)
uniform image2D outputImage;

layout(set=0, binding=3, std430)
buffer IndexBuffer
{
	uint indexBuffer[];
};

struct Vertex
{
	float position[3];
	float normal[3];
	float texcoord[2];
};

layout(set=0, binding=4, std430)
buffer VertexBuffer
{
	Vertex vertexBuffer[];
};

vec3 getPosition(Vertex v) { return vec3(v.position[0], v.position[1], v.position[2]); }
vec3 getNormal(Vertex v) { return vec3(v.normal[0], v.normal[1], v.normal[2]); }
vec2 getTexcoord(Vertex v) { return vec2(v.texcoord[0], v.texcoord[1]); }

layout(set=0, binding=5)
uniform accelerationStructureNV TLAS;

#define MaxTextures 1024
layout(set=1, binding = 0)
uniform texture2D textureDescriptors[MaxTextures];

// common types and functions

struct MaterialConstants
{
	vec4 baseColor;
	uint albedoTextureId;
	uint specularTextureId;
	uint firstIndex;
	uint alphaMode;
	float metallicFactor;
	float roughnessFactor;
};

struct DefaultPayload
{
	float hitT;

	vec3 albedo;
	
	float metalness;
	float roughness;

	vec3 normal;
	vec3 geoNormal;
};

struct RayDesc
{
	vec3 origin;
	float minT;
	vec3 direction;
	float maxT;
};

float nudgeULP(float x, int delta)
{
	return uintBitsToFloat(floatBitsToUint(x) + delta);
}

float max3(vec3 v)
{
	return max(max(v.x, v.y), v.z);
}

float randomFloat(inout uint seed)
{
	seed = 214013 * seed + 2531011;
	return float(seed >> 16) * (1.0f / 65535.0f);
}

vec2 randomFloat2(inout uint seed)
{
	float x = randomFloat(seed);
	float y = randomFloat(seed);
	return vec2(x, y);
}

uint hashFnv1(uint x)
{
	uint state = 0x811c9dc5;
	for (uint i = 0; i < 4; ++i)
	{
		state *= 0x01000193;
		state ^= (x & 0xFF);
		x = x >> 8;
	}
	return state;
}

vec3 mapToCosineHemisphere(vec2 uv)
{
	float phi = uv.x * M_PI * 2.0;
	vec2 scTheta = vec2(sqrt(uv.y), sqrt(1.0-uv.x));
	vec2 scPhi = vec2(sin(phi), cos(phi));

	return vec3(
		scPhi.x * scTheta.x, 
		scPhi.y * scTheta.x, 
		scTheta.y);
}

vec3 mapToUniformSphere(vec2 uv)
{
	float z = 1.0 - 2.0 * uv.x;
	float r = sqrt(1.0 - z*z);
	float phi = 2.0f * M_PI * uv.y;
	float x = cos(phi);
	float y = sin(phi);
	return vec3(x,y,z);
}

mat3 makeOrthonormalBasis(vec3 n)
{
	vec3 u = abs(dot(n, vec3(0,1,0))) < 0.9 ? vec3(0,1,0) : vec3(1,0,0);
	vec3 z = n;
	vec3 x = normalize(cross(u, z));
	vec3 y = cross(z, x);
	return mat3(x, y, z);
}


#endif // INCLUDED_COMMON_GLSL
