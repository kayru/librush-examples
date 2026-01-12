cbuffer Global : register(b0, space0)
{
	row_major float4x4 g_matViewProj;
};

static const uint maxBatchCount = 1000;

cbuffer Instance : register(b1, space0)
{
	row_major float4x4 g_matWorld[maxBatchCount];
};

struct VSInput
{
	float3 pos : POSITION;
	float4 color : COLOR0;
	uint instanceId : SV_InstanceID;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float4 color : COLOR0;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	float3 worldPos = mul(float4(input.pos, 1.0f), g_matWorld[input.instanceId]).xyz;
	output.pos = mul(float4(worldPos, 1.0f), g_matViewProj);
	output.color = input.color;
	return output;
}
