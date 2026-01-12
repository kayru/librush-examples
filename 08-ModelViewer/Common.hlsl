cbuffer SceneConstants : register(b0, space0)
{
	float4x4 g_matViewProj;
	float4x4 g_matWorld;
};

cbuffer MaterialConstants : register(b0, space1)
{
	float4 g_baseColor;
};

SamplerState sampler0 : register(s1, space1);
Texture2D texture0 : register(t2, space1);
