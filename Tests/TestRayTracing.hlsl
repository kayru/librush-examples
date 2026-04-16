SamplerState samplerNearest : register(s0, space0);
Texture2D textureArray[2] : register(t1, space0);
RWStructuredBuffer<uint> outputBuffer : register(u3, space0);
RaytracingAccelerationStructure TLAS : register(t4, space0);

uint packColor(float4 color)
{
	float4 clamped = clamp(color, 0.0, 1.0) * 255.0 + 0.5;
	uint4 rgba = uint4(clamped);
	return rgba.r | (rgba.g << 8) | (rgba.b << 16) | (rgba.a << 24);
}

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	// Trace two rays at different X positions to hit two different instances.
	// Instance 0 is at x=-1, instance 1 is at x=+1.
	float rayOffsets[2] = { -1.0, 1.0 };

	for (int i = 0; i < 2; ++i)
	{
		RayDesc rd;
		rd.Origin = float3(rayOffsets[i], 0.0, -5.0);
		rd.Direction = float3(0, 0, 1);
		rd.TMin = 0.0;
		rd.TMax = 100.0;

		RayQuery<RAY_FLAG_FORCE_OPAQUE> rq;
		rq.TraceRayInline(TLAS, RAY_FLAG_FORCE_OPAQUE, 0xFF, rd);
		while (rq.Proceed()) {}

		if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			uint texIdx = rq.CommittedInstanceID();
			float4 color = textureArray[NonUniformResourceIndex(texIdx)].SampleLevel(samplerNearest, float2(0.25, 0.25), 0);
			outputBuffer[i] = packColor(color);
		}
		else
		{
			outputBuffer[i] = 0;
		}
	}
}
