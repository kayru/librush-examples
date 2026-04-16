// Set 0: per-frame resources (sampler, output buffer, TLAS)
SamplerState samplerNearest : register(s0, space0);
RWStructuredBuffer<uint> outputBuffer : register(u1, space0);
RaytracingAccelerationStructure TLAS : register(t2, space0);

// Set 1: material texture array
Texture2D textureArray[4] : register(t0, space1);

uint packColor(float4 color)
{
	float4 clamped = clamp(color, 0.0, 1.0) * 255.0 + 0.5;
	uint4 rgba = uint4(clamped);
	return rgba.r | (rgba.g << 8) | (rgba.b << 16) | (rgba.a << 24);
}

[numthreads(4, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Each thread traces one ray. Instances are spaced 2 units apart along X,
	// starting at x=-3: instance 0 at -3, 1 at -1, 2 at +1, 3 at +3.
	float x = -3.0 + 2.0 * float(id.x);

	RayDesc rd;
	rd.Origin = float3(x, 0.0, -5.0);
	rd.Direction = float3(0, 0, 1);
	rd.TMin = 0.0;
	rd.TMax = 100.0;

	RayQuery<RAY_FLAG_FORCE_OPAQUE> rq;
	rq.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, rd);
	while (rq.Proceed()) {}

	if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		uint texIdx = rq.CommittedInstanceID();
		float4 color = textureArray[NonUniformResourceIndex(texIdx)].SampleLevel(
		    samplerNearest, float2(0.25, 0.25), 0);
		outputBuffer[id.x] = packColor(color);
	}
	else
	{
		outputBuffer[id.x] = 0;
	}
}
