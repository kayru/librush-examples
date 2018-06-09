#include "Common.hlsl"

float4 main(VSOutput v) : SV_Target
{
	return g_baseColor * texture0.Sample(sampler0, v.tex);
}
