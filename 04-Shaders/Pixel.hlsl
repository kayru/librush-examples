struct Constants
{
	float4 params;
};

cbuffer constantBuffer0 : register(b0)
{
	Constants globals;
};

float noise(float2 t, float s)
{
    return frac(sin(s+dot(t.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float bouncy(float2 v, float2 fragCoord, float4 params)
{
	float2 cp = v * params.z;
	float2 cp_wrap = float2(uint2(cp) / uint2(params.xy));
	cp = fmod(cp, params.xy);
	cp = lerp(cp, params.xy - cp, fmod(cp_wrap, float2(2.0, 2.0)));
	return 25.0 / (1.0+length(cp - fragCoord.xy));
}

float4 main(float4 fragCoord : SV_Position) : SV_Target
{
	float3 res = float3(0, 0, 0);
	res += float3(1.0, 0.3, 0.2) * bouncy(float2(211, 312), fragCoord.xy, globals.params);
	res += float3(0.3, 1.0, 0.2) * bouncy(float2(312, 210), fragCoord.xy, globals.params);
	res += float3(0.2, 0.3, 1.0) * bouncy(float2(331, 130), fragCoord.xy, globals.params);
	float2 p = fragCoord.xy;
	float dither = (noise(p, globals.params.z) - 0.5) / 64.0;
	return float4(res + dither, 1);
}
