#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

struct TestArgs
{
	device uint4* results [[id(0)]];
	device uint* materialIndices [[id(1)]];
	instance_acceleration_structure tlas [[id(2)]];
};

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
	uint materialIndex = 0u;
	if (hitResult.type != intersection_type::none)
	{
		const uint primId = hitResult.primitive_id;
		materialIndex = args.materialIndices[primId];
	}

	args.results[0] = uint4(materialIndex, 0u, 0u, 0u);
}
