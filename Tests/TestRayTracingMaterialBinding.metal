#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

struct MaterialConstants
{
	float4 albedoFactor;
	float4 specularFactor;
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

struct TestArgs
{
	sampler samplerState [[id(0)]];
	array<texture2d<float, access::sample>, 2> textures [[id(1)]];
	device uint4* results [[id(3)]];
	device MaterialConstants* materials [[id(4)]];
	device uint* materialIndices [[id(5)]];
	instance_acceleration_structure tlas [[id(6)]];
};

static inline uint packColor(float4 color)
{
	color = clamp(color, 0.0f, 1.0f);
	const uint r = uint(color.x * 255.0f + 0.5f);
	const uint g = uint(color.y * 255.0f + 0.5f);
	const uint b = uint(color.z * 255.0f + 0.5f);
	const uint a = uint(color.w * 255.0f + 0.5f);
	return r | (g << 8) | (b << 16) | (a << 24);
}

kernel void main0(constant TestArgs& args [[buffer(0)]])
{
	intersector<triangle_data, instancing> intersector;
	intersector.assume_geometry_type(geometry_type::triangle);
	intersector.set_triangle_cull_mode(triangle_cull_mode::none);

	ray hitRay;
	hitRay.origin = float3(0.0f, 0.0f, -1.0f);
	hitRay.direction = float3(0.0f, 0.0f, 1.0f);
	hitRay.min_distance = 0.001f;
	hitRay.max_distance = 10000.0f;

	intersection_result<triangle_data, instancing> hitResult = intersector.intersect(hitRay, args.tlas);
	uint packed = 0u;
	uint materialIndex = 0u;
	if (hitResult.type != intersection_type::none)
	{
		materialIndex = args.materialIndices[hitResult.primitive_id];
		MaterialConstants mat = args.materials[materialIndex];
		float4 color = args.textures[mat.albedoTextureId].sample(args.samplerState, float2(0.5f, 0.5f));
		packed = packColor(color);
	}

	args.results[0] = uint4(packed, materialIndex, 0u, 0u);
}
