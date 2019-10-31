#ifndef COMMON_HLSL
#define COMMON_HLSL

// descriptor set 0

cbuffer constantBuffer0 : register(b0, space0)
{
	float4x4 g_matViewProj;
	float4x4 g_matWorld;
};

SamplerState sampler0 : register(s1, space0);

// descriptor set 1

cbuffer constantBuffer1 : register(b0, space1)
{
	float4 g_baseColor;
};

Texture2D<float4> texture0 : register(t1, space1);

// 

struct VSOutput
{
	float4 pos : SV_Position;
	float3 nor : NORMAL0;
	float2 tex : TEXCOORD0;
};

#endif // COMMON_HLSL