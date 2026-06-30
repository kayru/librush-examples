#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

#include "PathTracerConstants.glsl"

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
//  7 focusFeedbackBuffer
//  8 TLAS (Vulkan)
// Metal argument buffers follow the same ordering; when Metal-only material buffers
// are bound, they occupy slots 7/8, focusFeedback is 9, and TLAS shifts to 10.
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

// click-to-focus: cursor pixel writes its primary-hit depth here
layout(set = 0, binding = 7, std430)
buffer FocusFeedbackBuffer
{
	float focusFeedback[];
};

vec3 getPosition(Vertex v) { return vec3(v.position[0], v.position[1], v.position[2]); }
vec3 getNormal(Vertex v) { return vec3(v.normal[0], v.normal[1], v.normal[2]); }
vec2 getTexcoord(Vertex v) { return vec2(v.texcoord[0], v.texcoord[1]); }
vec4 getTangent(Vertex v) { return vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]); }

layout(set=0, binding=8)
uniform accelerationStructureEXT TLAS;

layout(set=1, binding = 0)
uniform texture2D textureDescriptors[PT_MAX_TEXTURES];

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

#include "PathTracerContext.glsl"
#include "PathTracerCore.glsl"

#endif // __cplusplus

#endif // INCLUDED_COMMON_GLSL
