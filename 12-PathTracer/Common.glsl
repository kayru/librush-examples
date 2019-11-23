#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

#define PT_FLAG_USE_ENVMAP              (1 << 0)
#define PT_FLAG_USE_NEUTRAL_BACKGROUND  (1 << 1)

#define PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS   0
#define PT_MATERIAL_MODE_PBR_SPECULAR_GLOSSINESS  1

#ifndef __cplusplus

#define M_PI 3.14159265358979323846264338327950288
#define saturate(x) clamp(x, 0.0, 1.0)

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
	uint flags;
};

layout(set=0, binding=1)
uniform sampler defaultSampler;

layout(set=0, binding=2)
uniform texture2D envmapTexture;

layout(set=0, binding=3, rgba32f)
uniform image2D outputImage;

layout(set=0, binding=4, std430)
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

layout(set=0, binding=5, std430)
buffer VertexBuffer
{
	Vertex vertexBuffer[];
};

vec3 getPosition(Vertex v) { return vec3(v.position[0], v.position[1], v.position[2]); }
vec3 getNormal(Vertex v) { return vec3(v.normal[0], v.normal[1], v.normal[2]); }
vec2 getTexcoord(Vertex v) { return vec2(v.texcoord[0], v.texcoord[1]); }

layout(set=0, binding=6)
uniform accelerationStructureNV TLAS;

#define MaxTextures 1024
layout(set=1, binding = 0)
uniform texture2D textureDescriptors[MaxTextures];

// common types and functions

struct MaterialConstants
{
	vec4 albedoFactor;
	vec4 specularFactor;
	uint albedoTextureId;
	uint specularTextureId;
	uint firstIndex;
	uint alphaMode;
	float metallicFactor;
	float roughnessFactor;
	float reflectance;
	uint materialMode;
};

struct DefaultPayload
{
	float hitT;

	vec3 baseColor;
	float metalness;
	float roughness;
	float reflectance;

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

float pow5(float x)
{
	float x2 = x * x;
	return x2 * x2 * x;
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

// Point on hemisphere surface around Z+ with cosine weighted distribution
vec3 mapToCosineHemisphere(vec2 uv)
{
	float r = sqrt(uv.x);
	float theta = 2.0 * M_PI * uv.y;
	float x = r * cos(theta);
	float y = r * sin(theta);
	return vec3(x, y, sqrt(1.0 - uv.x));
}
vec3 sampleCosineHemisphere(inout uint seed) { return mapToCosineHemisphere(randomFloat2(seed)); };

// Point on hemisphere surface around Z+
vec3 mapToUniformHemisphere(vec2 uv)
{
	float r = sqrt(1.0 - uv.x * uv.x);
	float phi = 2.0 * M_PI * uv.y;
	return vec3(cos(phi) * r, sin(phi) * r, uv.x);
}
vec3 sampleUniformHemisphere(inout uint seed) { return mapToUniformHemisphere(randomFloat2(seed)); };

// Point on sphere surface
vec3 mapToUniformSphere(vec2 uv)
{
	float phi = uv.x * M_PI * 2.0;
	float z = 1.0 - 2.0 * uv.y;
	float r = sqrt(1.0 - z*z);
	float x = r * cos(phi);
	float y = r * sin(phi);
	return vec3(x,y,z);
}
vec3 sampleUniformSphere(inout uint seed) { return mapToUniformSphere(randomFloat2(seed)); };

vec3 sampleUniformSphereInterior(inout uint seed)
{ 
	for(;;)
	{
		vec3 v;
		v.x = randomFloat(seed);
		v.y = randomFloat(seed);
		v.z = randomFloat(seed);
		if (dot(v,v) <= 1.0)
		{
			return v;
		}
	}
}

mat3 makeOrthonormalBasis(vec3 n)
{
	vec3 u = abs(dot(n, vec3(0,1,0))) < 0.9 ? vec3(0,1,0) : vec3(1,0,0);
	vec3 z = n;
	vec3 x = normalize(cross(u, z));
	vec3 y = cross(z, x);
	return mat3(x, y, z);
}

struct Surface
{
	vec3 normal;
	vec3 diffuseColor;
	vec3 specularColor;
	float linearRoughness;
};

Surface unpack(DefaultPayload p)
{
	Surface result;
	result.normal = p.normal;
	result.diffuseColor = p.baseColor - p.baseColor * p.metalness;
	result.specularColor = p.baseColor * p.metalness + (p.reflectance * (1.0 - p.metalness));
	result.linearRoughness = max(0.0001, p.roughness * p.roughness);
	return result;	
}

// Halton sequence implementation by Ollj
// https://www.shadertoy.com/view/tl2GDw

float Halton(int b, int i)
{
	float r = 0.0;
	float f = 1.0;
	while (i > 0) 
	{
		f = f / float(b);
		r = r + f * float(i % b);
		i = int(floor(float(i) / float(b)));
	}
	return r;
}

vec2 Halton23(int i)
{
	return vec2(Halton(2, i), Halton(3, i));
}

vec2 cartesianToLatLongTexcoord(vec3 p)
{
	// http://gl.ict.usc.edu/Data/HighResProbes

	float u = (1.0f + atan(p.z, -p.x) / M_PI);
	float v = acos(p.y) / M_PI;

	return vec2(u * 0.5f, v);
}

#endif // __cplusplus

#endif // INCLUDED_COMMON_GLSL
