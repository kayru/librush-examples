#ifndef COMMON_HLSL
#define COMMON_HLSL

cbuffer constantBuffer0 : register(b0)
{
	float4x4 g_matViewProj;
	float4x4 g_matWorld;
};

cbuffer constantBuffer1 : register(b1)
{
	float4 g_baseColor;
};

SamplerState sampler0 : register(s2);
Texture2D<float4> texture0 : register(t3);

struct VSOutput
{
	float4 pos : SV_Position;
	float3 nor : NORMAL0;
	float2 tex : TEXCOORD0;
};

#endif // COMMON_HLSL