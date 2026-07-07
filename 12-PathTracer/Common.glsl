#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

#include "PathTracerConstants.glsl"

#ifndef __cplusplus

// Inline tracing (ray query compute) indexes the bindless texture array with a per-thread
// material id, which is not dynamically uniform; nonuniformEXT is required for correct sampling.
#extension GL_EXT_nonuniform_qualifier : require

#extension GL_EXT_scalar_block_layout : require

#include "ShaderShared.glsl"

// global resources
// Binding layout (set=0), SBT ray-tracing-pipeline configs (rgen/rchit/rmiss):
//  0 SceneConstants
//  1 defaultSampler
//  2 envmapTexture
//  3 outputImage
//  4 indexBuffer
//  5 vertexBuffer
//  6 envmapDistributionBuffer
//  7 focusFeedbackBuffer
//  8 sobolBuffer
//  9 TLAS
// Inline configs (Vulkan ray query, PT_CONFIG_RAYQUERY) resolve materials from buffers rather than
// the SBT, so materials + material indices sit at 7/8, focus shifts to 9, sobol to 10, TLAS to 11.
// Metal argument buffers follow the same inline ordering (materials 7, indices 8, focus 9, sobol 10, TLAS 11).
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

	ivec2 focusPickPixel; // cursor pixel; x < 0 = no pick
	float focalPlaneFalloffPx;
	uint normalMapBounceLimit;

	// Single rectangular area light (PT_FLAG_USE_AREA_LIGHT): point(u,v) = origin + u*edgeU + v*edgeV.
	vec4 areaLightOrigin;   // xyz corner
	vec4 areaLightEdgeU;    // xyz first edge
	vec4 areaLightEdgeV;    // xyz second edge
	vec4 areaLightEmission; // xyz emitted radiance

	uint samplerMode;       // PT_SAMPLER_*
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

struct MaterialConstants
{
	vec4 albedoFactor;
	vec4 specularFactor;
	vec4 emissiveFactor;
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

#ifdef PT_CONFIG_RAYQUERY

layout(set=0, binding=7, scalar)
buffer MaterialBuffer
{
	MaterialConstants materials[];
};

layout(set=0, binding=8, std430)
buffer MaterialIndexBuffer
{
	uint materialIndices[];
};

// click-to-focus: cursor pixel writes its primary-hit depth here
layout(set=0, binding=9, std430)
buffer FocusFeedbackBuffer
{
	float focusFeedback[];
};

layout(set=0, binding=10, std430)
buffer SobolBuffer
{
	uint sobolBuffer[];
};

layout(set=0, binding=11)
uniform accelerationStructureEXT TLAS;

#else

// click-to-focus: cursor pixel writes its primary-hit depth here
layout(set=0, binding=7, std430)
buffer FocusFeedbackBuffer
{
	float focusFeedback[];
};

layout(set=0, binding=8, std430)
buffer SobolBuffer
{
	uint sobolBuffer[];
};

layout(set=0, binding=9)
uniform accelerationStructureEXT TLAS;

#endif

layout(set=1, binding = 0)
uniform texture2D textureDescriptors[PT_MAX_TEXTURES];

// common types and functions

#include "PathTracerContext.glsl"
#include "PathTracerSampling.glsl"
#include "PathTracerCore.glsl"

#endif // __cplusplus

#endif // INCLUDED_COMMON_GLSL
