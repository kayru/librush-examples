RWStructuredBuffer<uint> output : register(u0, space0);

[numthreads(64, 1, 1)]
void main(uint tid : SV_DispatchThreadID)
{
	uint x = tid;
	for (uint i = 0; i < 4096; ++i)
	{
		x = x * 1664525u + 1013904223u;
	}
	output[tid] = x;
}
