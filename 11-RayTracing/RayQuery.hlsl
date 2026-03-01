cbuffer Constants : register(b0, space0) { int2 outputSize; };
[[vk::image_format("rgba16f")]] RWTexture2D<float4> outputImage : register(u1, space0);
RaytracingAccelerationStructure TLAS : register(t2, space0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	int2 pixelIndex = int2(id.xy);
	if (pixelIndex.x >= outputSize.x || pixelIndex.y >= outputSize.y) return;
	float2 pixelPos = (float2(pixelIndex) + 0.5) / float2(outputSize);
	float3 rayOrigin = float3((pixelPos - 0.5) * float2(2, -2), 0);
	float3 rayDir = float3(0, 0, 1);
	RayDesc rd = { rayOrigin, 0.0, rayDir, 1e9 };
	RayQuery<RAY_FLAG_FORCE_OPAQUE> rq;
	rq.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, rd);
	while (rq.Proceed()) {}
	float3 color = float3(pixelPos, 0.0) * 0.25;
	if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		float2 bary = rq.CommittedTriangleBarycentrics();
		color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
	}
	outputImage[pixelIndex] = float4(color, 1.0);
}
