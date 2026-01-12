cbuffer Constants : register(b0, space0)
{
	float4 params;
};

struct PSInput
{
	float4 pos : SV_Position;
};

float noise(float2 t, float s)
{
	return frac(sin(s + dot(t, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float bouncy(float2 v, float2 fragCoord, float4 params)
{
	float2 cp = v * params.z;
	float2 cp_wrap = floor(cp / params.xy);
	cp = fmod(cp, params.xy);
	cp = lerp(cp, params.xy - cp, fmod(cp_wrap, 2.0f));
	return 25.0f / (1.0f + length(cp - fragCoord));
}

float4 main(PSInput input) : SV_Target
{
	float3 res = float3(0.0f, 0.0f, 0.0f);
	float2 fragCoord = input.pos.xy;
	res += float3(1.0f, 0.3f, 0.2f) * bouncy(float2(211.0f, 312.0f), fragCoord, params);
	res += float3(0.3f, 1.0f, 0.2f) * bouncy(float2(312.0f, 210.0f), fragCoord, params);
	res += float3(0.2f, 0.3f, 1.0f) * bouncy(float2(331.0f, 130.0f), fragCoord, params);
	float dither = (noise(fragCoord, params.z) - 0.5f) / 64.0f;
	return float4(res + dither, 1.0f);
}
