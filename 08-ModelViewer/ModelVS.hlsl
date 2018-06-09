#include "Common.hlsl"

VSOutput main(
	float3 a_pos0 : POSITION0,
	float3 a_nor0 : NORMAL0,
	float2 a_tex0 : TEXCOORD0)
{
	VSOutput result;

	float3 worldPos = mul(float4(a_pos0, 1), g_matWorld).xyz;

	result.pos = mul(float4(worldPos, 1), g_matViewProj);

	result.tex = a_tex0;
	result.nor = a_nor0;

	return result;
}
