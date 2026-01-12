#include "Common.hlsl"

struct VSInput
{
	float3 pos : POSITION;
	float3 nor : NORMAL0;
	float2 tex : TEXCOORD0;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 nor : TEXCOORD0;
	float2 tex : TEXCOORD1;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 worldPos = mul(float4(input.pos, 1.0f), g_matWorld).xyz;
	output.pos = mul(float4(worldPos, 1.0f), g_matViewProj);
	output.tex = input.tex;
	output.nor = input.nor;
	return output;
}
