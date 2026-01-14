#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

struct TestArgs
{
	device uint4* results [[id(0)]];
	device packed_float3* vertices [[id(1)]];
	device uint* indices [[id(2)]];
	instance_acceleration_structure tlas [[id(3)]];
};

[[visible]] uint missShader()
{
	return 1u;
}

[[visible]] uint closestHitShader(intersection_result<triangle_data, instancing> hit,
	device packed_float3* vertices,
	device uint* indices)
{
	(void)hit;
	const uint i0 = indices[0];
	const uint i1 = indices[1];
	const uint i2 = indices[2];
	const float3 v0 = float3(vertices[i0]);
	const float3 v1 = float3(vertices[i1]);
	const float3 v2 = float3(vertices[i2]);
	const uint value = uint(fabs(v0.x) + fabs(v1.y) + fabs(v2.x));
	return value;
}

kernel void main0(constant TestArgs& args [[buffer(0)]])
{
	intersector<triangle_data, instancing> intersector;
	intersector.assume_geometry_type(geometry_type::triangle);
	intersector.set_triangle_cull_mode(triangle_cull_mode::none);

	instance_acceleration_structure tlas = args.tlas;

	const uint i0 = args.indices[0];
	const uint i1 = args.indices[1];
	const uint i2 = args.indices[2];
	const float3 v0 = float3(args.vertices[i0]);
	const float3 v1 = float3(args.vertices[i1]);
	const float3 v2 = float3(args.vertices[i2]);
	const uint bufferValue = uint(fabs(v0.x) + fabs(v1.y) + fabs(v2.x));

	ray missRay;
	missRay.origin = float3(2.0f, 2.0f, -1.0f);
	missRay.direction = float3(0.0f, 0.0f, 1.0f);
	missRay.min_distance = 0.001f;
	missRay.max_distance = 10000.0f;
	intersection_result<triangle_data, instancing> missResult = intersector.intersect(missRay, tlas);
	uint missValue = (missResult.type == intersection_type::none) ? missShader() : 0u;

	args.results[0] = uint4(bufferValue, missValue, 0u, 0u);
}
