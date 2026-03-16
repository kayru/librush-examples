SamplerState samplerNearest : register(s0, space0);
SamplerState samplerLinear  : register(s1, space0);
SamplerState samplerUnused  : register(s2, space0);
Texture2D textureDescriptors[2] : register(t3, space0);
RWStructuredBuffer<uint4> outputBuffer : register(u4, space0);

uint packColor(float4 color)
{
	float4 clamped = clamp(color, 0.0, 1.0) * 255.0 + 0.5;
	uint4 rgba = uint4(clamped);
	return rgba.r | (rgba.g << 8) | (rgba.b << 16) | (rgba.a << 24);
}

[numthreads(2, 2, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	float2 uvNearest = float2(0.25, 0.25);
	float2 uvLinear  = float2(0.5, 0.5);

	float4 nearest0 = textureDescriptors[0].Sample(samplerNearest, uvNearest);
	float4 nearest1 = textureDescriptors[1].Sample(samplerNearest, uvNearest);
	float4 linear0  = textureDescriptors[0].Sample(samplerLinear, uvLinear);
	float4 linear1  = textureDescriptors[1].Sample(samplerLinear, uvLinear);

	outputBuffer[0] = uint4(
		packColor(nearest0),
		packColor(nearest1),
		packColor(linear0),
		packColor(linear1));
}
