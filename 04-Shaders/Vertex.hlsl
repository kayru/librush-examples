struct VSInput
{
	float2 pos : POSITION;
};

struct VSOutput
{
	float4 pos : SV_Position;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	output.pos = float4(input.pos, 0.0f, 1.0f);
	return output;
}
