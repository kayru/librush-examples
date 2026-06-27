#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

#define PT_FLAG_USE_ENVMAP              (1 << 0)
#define PT_FLAG_USE_NEUTRAL_BACKGROUND  (1 << 1)
#define PT_FLAG_USE_DEPTH_OF_FIELD      (1 << 2)
#define PT_FLAG_USE_NORMAL_MAPPING      (1 << 3)
#define PT_FLAG_DEBUG_SIMPLE_SHADING    (1 << 4)
#define PT_FLAG_DEBUG_DISABLE_ACCUMULATION (1 << 5)
#define PT_FLAG_DEBUG_HIT_MASK          (1 << 6)
#define PT_FLAG_DEBUG_FOCAL_PLANE       (1 << 7)

#define PT_DEBUG_VIS_NONE               0u
#define PT_DEBUG_VIS_ALBEDO             1u
#define PT_DEBUG_VIS_GEO_NORMAL         2u
#define PT_DEBUG_VIS_SHADING_NORMAL     3u
#define PT_DEBUG_VIS_NORMAL_MAPPED      4u
#define PT_DEBUG_VIS_TANGENT            5u
#define PT_DEBUG_VIS_BITANGENT          6u
#define PT_DEBUG_VIS_METALNESS          7u
#define PT_DEBUG_VIS_ROUGHNESS          8u
#define PT_DEBUG_VIS_UV                 9u

#define PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS   0
#define PT_MATERIAL_MODE_PBR_SPECULAR_GLOSSINESS  1

#ifndef __cplusplus

#include "ShaderShared.glsl"

// global resources
// Binding layout (set=0):
//  0 SceneConstants
//  1 defaultSampler
//  2 envmapTexture
//  3 outputImage
//  4 indexBuffer
//  5 vertexBuffer
//  6 envmapDistributionBuffer
//  7 TLAS (Vulkan)
// Metal argument buffers follow the same ordering; when Metal-only material buffers
// are bound, they occupy slots 7/8, and TLAS shifts to 9.
// Binding layout (set=1): texture array at binding 0.

layout(set=0, binding=0)
uniform SceneConstants
{
	mat4 matView;
	mat4 matProj;
	mat4 matViewProj;
	mat4 matViewProjInv;
	mat4 matEnvmapTransform;
	vec4 cameraPosition;

	ivec2 outputSize;
	uint frameIndex;
	uint flags;

	ivec2 envmapSize;
	vec2 cameraSensorSize;

	float focalLength;
	float focusDistance;
	float apertureSize;
	uint debugVisMode;

	float focalPlaneFalloffPx;
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
	float tangent[4];
};

layout(set=0, binding=5, std430)
buffer VertexBuffer
{
	Vertex vertexBuffer[];
};

struct EnvmapCell
{
	float p;
	uint i;
};

layout(set = 0, binding = 6, std430)
buffer EnvmapDistributionBuffer
{
	EnvmapCell envmapDistributionBuffer[];
};

vec3 getPosition(Vertex v) { return vec3(v.position[0], v.position[1], v.position[2]); }
vec3 getNormal(Vertex v) { return vec3(v.normal[0], v.normal[1], v.normal[2]); }
vec2 getTexcoord(Vertex v) { return vec2(v.texcoord[0], v.texcoord[1]); }
vec4 getTangent(Vertex v) { return vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]); }

layout(set=0, binding=7)
uniform accelerationStructureEXT TLAS;

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
	uint normalTextureId;
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

float pow2(float x)
{
	return x*x;
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

// mapToUniformSphere and sampleUniformDisk live in ShaderShared.glsl
vec3 sampleUniformSphere(inout uint seed) { return mapToUniformSphere(randomFloat2(seed)); };

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

vec2 cartesianToLatLongTexcoord(vec3 p)
{
	// http://gl.ict.usc.edu/Data/HighResProbes

	float u = (1.0f + atan(p.z, -p.x) / M_PI);
	float v = acos(p.y) / M_PI;

	return vec2(u * 0.5f, v);
}

vec3 latLongTexcoordToCartesian(vec2 uv)
{
	// http://gl.ict.usc.edu/Data/HighResProbes

	float theta = M_PI*(uv.x*2.0 - 1.0);
	float phi = M_PI*uv.y;

	float x = sin(phi)*sin(theta);
	float y = cos(phi);
	float z = -sin(phi)*cos(theta);

	return vec3(z, y, x);
}

vec3 envMapPixelIndexToDirection(uint i, vec2 pixelJitter)
{
	uint x = i % envmapSize.x;
	uint y = i / envmapSize.x;

	vec2 pixelPos = vec2(x, y);
	vec2 uv = (pixelPos + pixelJitter) / vec2(envmapSize);

	return latLongTexcoordToCartesian(uv);
}

vec3 importanceSampleSkyLightDir(inout uint randomSeed)
{
	uint i = randomUint32(randomSeed) % (envmapSize.x * envmapSize.y);
	EnvmapCell entry = envmapDistributionBuffer[i];
	vec2 jitter = randomFloat2(randomSeed); // jitter within a texel

	if (randomFloat(randomSeed) <= entry.p)
	{
		return envMapPixelIndexToDirection(i, jitter);
	}
	else
	{
		return envMapPixelIndexToDirection(entry.i, jitter);
	}
}

float balanceHeuristic(float f, float g)
{
	return f / (f+g);
}

// powerHeuristic and focalPlaneOverlay live in ShaderShared.glsl

#endif // __cplusplus

#endif // INCLUDED_COMMON_GLSL
