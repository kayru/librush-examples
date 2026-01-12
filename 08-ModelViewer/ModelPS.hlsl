#include "Common.hlsl"

struct PSInput
{
	float3 nor : TEXCOORD0;
	float2 tex : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target
{
	float4 texColor = texture0.Sample(sampler0, input.tex);
	return g_baseColor * texColor;
}
