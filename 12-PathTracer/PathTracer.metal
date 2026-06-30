#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

#include "PathTracerConstants.glsl"
#include "ShaderShared.glsl"

// D_GGX, G1_Smith, F_Schlick and importanceSampleDGGXVNDF live in ShaderShared.glsl.

// cartesianToLatLongTexcoord and latLongTexcoordToCartesian live in ShaderShared.glsl.

// getSunDirection, getSunColor and getSkyColor live in ShaderShared.glsl

struct SceneConstants
{
	float4x4 matView;
	float4x4 matProj;
	float4x4 matViewProj;
	float4x4 matViewProjInv;
	float4x4 matEnvmapTransform;
	float4 cameraPosition;

	int2 outputSize;
	uint frameIndex;
	uint flags;

	int2 envmapSize;
	float2 cameraSensorSize;

	float focalLength;
	float focusDistance;
	float apertureSize;
	uint debugVisMode;

	int2 focusPickPixel; // cursor pixel; x < 0 = no pick
	float focalPlaneFalloffPx;
};

struct MaterialConstants
{
	packed_float4 albedoFactor;
	packed_float4 specularFactor;
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

struct Vertex
{
	packed_float3 position;
	packed_float3 normal;
	packed_float2 texcoord;
	packed_float4 tangent;
};

struct EnvmapCell
{
	float p;
	uint i;
};

struct PathTracerSet0
{
	constant SceneConstants* scene [[id(0)]];
	sampler defaultSampler [[id(1)]];
	texture2d<float, access::sample> envmapTexture [[id(2)]];
	texture2d<float, access::read_write> outputImage [[id(3)]];
	device uint* indexBuffer [[id(4)]];
	device Vertex* vertexBuffer [[id(5)]];
	device EnvmapCell* envmapDistribution [[id(6)]];
	device MaterialConstants* materials [[id(7)]];
	device uint* materialIndices [[id(8)]];
	device float* focusFeedback [[id(9)]];
	instance_acceleration_structure tlas [[id(10)]];
};

struct PathTracerSet1
{
	array<texture2d<float, access::sample>, PT_MAX_TEXTURES> textures [[id(0)]];
};

#include "PathTracerContext.glsl"
#include "PathTracerCore.glsl"

kernel void main0(constant PathTracerSet0& set0 [[buffer(0)]],
	constant PathTracerSet1& set1 [[buffer(1)]],
	uint2 gid [[thread_position_in_grid]])
{
	const uint width = set0.outputImage.get_width();
	const uint height = set0.outputImage.get_height();
	if (gid.x >= width || gid.y >= height)
	{
		return;
	}

	if (!set0.scene)
	{
		set0.outputImage.write(float4(0.0f, 0.0f, 0.0f, 1.0f), gid);
		return;
	}

	PathTracerContext ctx;
	ctx.s0 = &set0;
	ctx.s1 = &set1;
	ptRenderPixel(ctx, int2(gid));
}
